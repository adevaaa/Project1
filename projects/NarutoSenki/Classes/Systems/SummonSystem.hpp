#pragma once
#include <cmath>
#include "Core/Provider.hpp"
#include "GameLayer.h"
#include "HudLayer.h"
#include "Systems/AiCompat.hpp"
#include "Systems/SkillLock.hpp"
#include "Systems/SummonConfig.hpp"

#include <set>

/**
 * SummonSystem
 *
 * Logika Ai Summon saat game berjalan. Konfigurasi dibaca dari lua/class/AiCompat.lua
 * (#Kugutsu / #Kuchiyose / #Ninja / #Clone), lihat SummonConfig.hpp.
 *
 * Titik sambung ke kode lama:
 *   CharacterBase::setClone        -> SummonSystem::create + attach   (Id, closing)
 *   CharacterBase::setMonAttack    -> SummonSystem::onMonAttack       (setMonAttack = n = skillNN)
 *   CharacterBase::attack          -> SummonSystem::isSkillLocked     (lock / unlock / Skills = Lock)
 *   CharacterBase::findEnemyBy/2By -> SummonSystem::acceptTarget      (target / untarget / Area)
 *   Hero::setAI                    -> CharacterBase::luaSummonTick    (Attack = skillNN otomatis)
 *   Hero::dealloc                  -> SummonSystem::onGone
 *   Provider::create               -> SummonSystem::applyBaseline     (kunci awal milik "unlock")
 */
class SummonSystem
{
public:
	static Role roleOf(SummonKind kind)
	{
		switch (kind)
		{
		case SummonKind::Kuchiyose:
			return Role::Summon;
		case SummonKind::Kugutsu:
			return Role::Kugutsu;
		default:
			return Role::Clone;
		}
	}

	// skillNN -> ABType. Hanya skill01..03 yang bisa dijalankan clone (skill04/05 = ougis milik karakter utama).
	static bool toABType(int skill, ABType &out)
	{
		switch (skill)
		{
		case 1:
			out = SKILL1;
			return true;
		case 2:
			out = SKILL2;
			return true;
		case 3:
			out = SKILL3;
			return true;
		default:
			warnUnsupported(skill);
			return false;
		}
	}

	// ---- membuat & mendaftarkan summon ----

	static Hero *create(CharacterBase *owner, const SummonDef &def)
	{
		const auto *decl = SummonConfig::unit(def.unit);
		if (!decl)
			CCLOG("[SummonSystem] %s: unit %s belum dideklarasikan di #Kugutsu/#Kuchiyose/#Ninja, dianggap Ninja", owner->getName().c_str(), def.unit.c_str());

		const Role role = decl ? roleOf(decl->kind) : Role::Clone;
		Hero *hero = Provider::create(def.unit, role, owner->getGroup());
		if (!hero)
		{
			CCLOG("[SummonSystem] %s: gagal membuat %s", owner->getName().c_str(), def.unit.c_str());
			return nullptr;
		}
		hero->_luaDef = &def;
		if (role != Role::Clone)
			hero->_isArmored = true; // sama seperti Kugutsu/Kuchiyose bawaan
		return hero;
	}

	// Dipanggil setelah clone masuk ke layer dan doAI().
	static void attach(CharacterBase *owner, Hero *clone, const SummonDef &def, int xmlValue)
	{
		owner->_luaSummons.push_back({clone, &def});

		// owner mati -> Hero::dealloc ikut mematikan semua anggota _monsterArray
		bool known = false;
		for (auto mo : owner->_monsterArray)
			if (mo == clone)
				known = true;
		if (!known)
			owner->_monsterArray.push_back(clone);

		if (def.closing == SummonClosing::Timer)
		{
			if (xmlValue > 0)
				clone->scheduleOnce(schedule_selector(CharacterBase::removeClone), (float)xmlValue);
			else
				CCLOG("[SummonSystem] %s -> %s: closing = timer tetapi nilai setClone di XML %d (tidak ada durasi), clone tidak dibatasi waktu",
					  owner->getName().c_str(), def.unit.c_str(), xmlValue);
		}

		recomputeLocks(owner);
	}

	// Dipanggil Hero::dealloc untuk clone/kugutsu/summon.
	static void onGone(CharacterBase *clone)
	{
		if (!clone->_luaDef)
			return;
		CharacterBase *owner = clone->getMaster();
		clone->_luaDef = nullptr;
		if (!owner)
			return;
		auto &list = owner->_luaSummons;
		for (size_t i = 0; i < list.size(); ++i)
		{
			if (list[i].first == clone)
			{
				list.erase(list.begin() + i);
				break;
			}
		}
		recomputeLocks(owner);
	}

	// ---- setMonAttack = n = skillNN ----

	static void onMonAttack(CharacterBase *owner, int command)
	{
		if (owner->_luaSummons.empty())
			return;
		const auto list = owner->_luaSummons; // salinan: attack() bisa mengubah daftar
		for (auto &entry : list)
		{
			CharacterBase *c = entry.first;
			const SummonDef *def = entry.second;
			if (!c || !def || c->_state == State::DEAD)
				continue;
			for (auto &ma : def->monAttack)
			{
				if (ma.command != command)
					continue;
				for (int s : ma.skills)
				{
					ABType type;
					if (toABType(s, type) && ready(c, s))
						c->attack(type);
				}
			}
		}
	}

	// ---- Attack = skillNN (otomatis saat cooldown siap) ----

	static void tick(CharacterBase *c)
	{
		const SummonDef *def = c->_luaDef;
		if (!def || def->attack.empty() || c->_state == State::DEAD || !c->isFreeState())
			return;

		CharacterBase *t = c->_mainTarget.get();
		if (!t || t->_state == State::DEAD)
			return;

		const Vec2 sp = t->getPosition() - c->getPosition();
		for (int s : def->attack)
		{
			ABType type;
			if (!toABType(s, type) || !ready(c, s))
				continue;
			const float rx = std::max<float>(rangeX(c, s), 16.f);
			const float ry = std::max<float>(rangeY(c, s), 32.f);
			if (std::abs(sp.x) <= rx && std::abs(sp.y) <= ry)
			{
				c->changeSide(sp);
				c->attack(type);
				return;
			}
		}
	}

	// ---- target / untarget / Area ----

	static bool acceptTarget(CharacterBase *self, const SummonDef &def, CharacterBase *t)
	{
		if (!def.target.empty())
		{
			bool any = false;
			for (auto &k : def.target)
				if (isKind(t, k))
					any = true;
			if (!any)
				return false;
		}
		for (auto &k : def.untarget)
			if (isKind(t, k))
				return false;

		if (def.hasArea())
		{
			CharacterBase *owner = self->getMaster();
			if (owner && std::abs(t->getPositionX() - owner->getPositionX()) > def.radius() + 32)
				return false;
		}
		return true;
	}

	// ---- lock / unlock / Skills = Lock ----

	static bool isSkillLocked(CharacterBase *c, ABType type)
	{
		int idx = 0;
		if (type == SKILL1)
			idx = 1;
		else if (type == SKILL2)
			idx = 2;
		else if (type == SKILL3)
			idx = 3;
		else if (type == OUGIS1)
			idx = 4;
		else if (type == OUGIS2)
			idx = 5;
		return idx > 0 && c->_luaSkillLock[idx];
	}

	// Kunci skill dihitung di SkillLock (gabungan #Clone dan #Buff).
	static void recomputeLocks(CharacterBase *owner) { SkillLock::recompute(owner); }

	static void applyBaseline(CharacterBase *owner) { SkillLock::applyBaseline(owner); }

private:
	static bool isKind(CharacterBase *t, const std::string &k)
	{
		if (k == "shinobi" || k == "hero" || k == "ninja" || k == "player")
			return t->isPlayerOrCom() && !t->isGuardian();
		if (k == "guardian")
			return t->isGuardian();
		if (k == "flog" || k == "warrior" || k == "minion")
			return t->isFlog();
		if (k == "tower")
			return t->isTower();
		if (k == "clone")
			return t->isClone();
		if (k == "kugutsu")
			return t->isKugutsu();
		if (k == "summon" || k == "kuchiyose")
			return t->isSummon();
		if (k == "mon")
			return t->isMon();
		return false;
	}

	static bool ready(CharacterBase *c, int skill)
	{
		switch (skill)
		{
		case 1:
			return c->_isCanSkill1;
		case 2:
			return c->_isCanSkill2;
		case 3:
			return c->_isCanSkill3;
		}
		return false;
	}

	static float rangeX(CharacterBase *c, int skill)
	{
		switch (skill)
		{
		case 1:
			return (float)c->_sAttackRangeX1;
		case 2:
			return (float)c->_sAttackRangeX2;
		case 3:
			return (float)c->_sAttackRangeX3;
		}
		return 0;
	}

	static float rangeY(CharacterBase *c, int skill)
	{
		switch (skill)
		{
		case 1:
			return (float)c->_sAttackRangeY1;
		case 2:
			return (float)c->_sAttackRangeY2;
		case 3:
			return (float)c->_sAttackRangeY3;
		}
		return 0;
	}

	static void warnUnsupported(int skill)
	{
		static std::set<int> warned;
		if (warned.insert(skill).second)
			CCLOG("[SummonSystem] skill%02d dilewati: clone hanya mendukung skill01..skill03", skill);
	}
};
