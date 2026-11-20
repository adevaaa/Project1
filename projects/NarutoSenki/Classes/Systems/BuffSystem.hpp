#pragma once
#include "Core/Provider.hpp"
#include "Systems/AiCompat.hpp"
#include "Systems/BuffConfig.hpp"
#include "Systems/SkillLock.hpp"

/**
 * BuffSystem
 *
 * setBuff dengan id bebas lewat section #Buff di lua/class/AiCompat.lua.
 *
 * Titik sambung ke kode lama:
 *   CharacterBase::setBuff            -> BuffSystem::apply          (nilai <e type="setBuff">id</e>)
 *   CharacterBase::fireActionResume   -> BuffSystem::onActionResume (closing = setActionResume)
 *   CharacterBase::luaBuffTimeout     -> BuffSystem::onTimeout      (closing = timer [> skillNN])
 *   CharacterBase::acceptAttack       -> flag _luaImmune            (Imun)
 *   Hero::dealloc                     -> BuffSystem::end            (karakter mati)
 *
 * skillNN = aksi ke-(7 + NN) di XML karakter (skill01 = aksi ke-8). XML dibaca sekali per karakter
 * saat buff pertama dipasang.
 */
class BuffSystem
{
public:
	// true jika pemilik punya definisi #Buff untuk id ini (kode setBuff lama dilewati).
	static bool apply(CharacterBase *c, int id, float duration)
	{
		const BuffDef *def = AiCompat::buffDef(c->getName(), id);
		if (!def)
			return false;

		if (c->_luaBuff)
			end(c, false); // pasang ulang: pulihkan dulu, lalu terapkan dari awal

		CCArray *xml = ensureXml(c);
		if (!xml)
		{
			CCLOG("[BuffSystem] %s: XML karakter tidak bisa dibaca, buff %d dibatalkan", c->getName().c_str(), id);
			return true;
		}

		BuffSnapshot &snap = c->_luaBuffSnap;
		snap = BuffSnapshot();
		snap.armored = c->_isArmored;

		// animasi
		if (auto *a = frames(xml, def->idle))
			c->setIdleAction(c->createAnimation(a, 5, true, false));
		if (auto *a = frames(xml, def->walk))
			c->setWalkAction(c->createAnimation(a, 10, true, false));
		if (auto *a = frames(xml, def->nattack))
			c->setNAttackAction(c->createAnimation(a, 10, false, true));
		if (auto *a = frames(xml, def->dead))
			c->setDeadAction(c->createAnimation(a, 10, false, false));
		if (auto *a = frames(xml, def->hurt))
			c->setHurtAction(c->createAnimation(a, 10, false, false));

		// skill baru: skill1 = skill10 membuka skill1_1 (aksi + jenis, damage, jarak, cooldown dari XML)
		for (int slot = 1; slot <= 5; ++slot)
		{
			if (def->skill[slot] <= 0)
				continue;
			auto *action = actionAt(xml, def->skill[slot]);
			CCArray *a = action ? (CCArray *)action->objectAtIndex(1) : nullptr;
			if (!a || a->count() == 0)
			{
				CCLOG("[BuffSystem] %s: skill%02d (untuk skill%d) kosong di XML, dilewati", c->getName().c_str(), def->skill[slot], slot);
				continue;
			}
			saveSkill(c, snap, slot);
			CCArray *data = (CCArray *)action->objectAtIndex(0);
			string type;
			uint32_t value = 0, cd = 0;
			int rx = 0, ry = 0, cp = 0;
			c->readData(data, type, value, rx, ry, cd, cp);
			writeSkill(c, slot, type, value, rx, ry, cd);
			setSkillAction(c, slot, c->createAnimation(a, 10, false, true));
		}

		if (def->armored)
			c->_isArmored = true;
		if (def->imun)
			c->_luaImmune = true;

		c->unschedule(schedule_selector(CharacterBase::luaBuffTimeout));
		if (def->closeTimer)
		{
			if (duration > 0)
				c->scheduleOnce(schedule_selector(CharacterBase::luaBuffTimeout), duration);
			else
				CCLOG("[BuffSystem] %s buff %d: closing = timer tetapi durasi (attackRangeY skill) 0, buff tidak dibatasi waktu", c->getName().c_str(), id);
		}

		c->_luaBuff = def;
		SkillLock::recompute(c);
		refreshIdle(c);
		return true;
	}

	// Dipanggil setActionResume di XML. true = buff ditutup olehnya (setActionResume bawaan dilewati).
	static bool onActionResume(CharacterBase *c)
	{
		if (!c->_luaBuff || !c->_luaBuff->closeResume)
			return false;
		end(c, false);
		return true;
	}

	static void onTimeout(CharacterBase *c)
	{
		if (c->_luaBuff)
			end(c, true);
	}

	// Akhiri buff: pulihkan animasi, skill, armored, imun, kunci. runEndSkill = jalankan "timer > skillNN".
	static void end(CharacterBase *c, bool runEndSkill)
	{
		const BuffDef *def = c->_luaBuff;
		if (!def)
			return;
		c->_luaBuff = nullptr;
		c->unschedule(schedule_selector(CharacterBase::luaBuffTimeout));

		if (frames(c->_luaXml, def->idle))
			c->setIdleAction(c->createAnimation(c->idleArray, 5, true, false));
		if (frames(c->_luaXml, def->walk))
			c->setWalkAction(c->createAnimation(c->walkArray, 10, true, false));
		if (frames(c->_luaXml, def->nattack))
			c->setNAttackAction(c->createAnimation(c->nattackArray, 10, false, true));
		if (frames(c->_luaXml, def->dead))
			c->setDeadAction(c->createAnimation(c->deadArray, 10, false, false));
		if (frames(c->_luaXml, def->hurt))
			c->setHurtAction(c->createAnimation(c->hurtArray, 10, false, false));

		BuffSnapshot &snap = c->_luaBuffSnap;
		for (int slot = 1; slot <= 5; ++slot)
		{
			if (!snap.has[slot])
				continue;
			writeSkill(c, slot, snap.type[slot], snap.value[slot], snap.rangeX[slot], snap.rangeY[slot], snap.cd[slot]);
			CCArray *base = baseArray(c, slot);
			if (base)
				setSkillAction(c, slot, c->createAnimation(base, 10, false, true));
		}

		c->_isArmored = snap.armored;
		c->_luaImmune = false;
		snap = BuffSnapshot();

		SkillLock::recompute(c);
		refreshIdle(c);

		if (runEndSkill && def->endSkill > 0 && c->_state != State::DEAD && c->isFreeState())
		{
			auto *action = actionAt(c->_luaXml, def->endSkill);
			CCArray *a = action ? (CCArray *)action->objectAtIndex(1) : nullptr;
			if (a && a->count() > 0)
			{
				string type;
				uint32_t value = 0, cd = 0;
				int rx = 0, ry = 0, cp = 0;
				c->readData((CCArray *)action->objectAtIndex(0), type, value, rx, ry, cd, cp);
				c->_attackType = type;
				c->_attackValue = value ? value : c->getNAttackValue();
				c->_attackRangeX = rx;
				c->_attackRangeY = ry;
				c->stopAllActions();
				c->_state = State::SATTACK;
				c->runAction(c->createAnimation(a, 10, false, true));
			}
		}
	}

private:
	// XML karakter dibaca sekali dan disimpan di CharacterBase::_luaXml (dilepas di ~Hero).
	static CCArray *ensureXml(CharacterBase *c)
	{
		if (c->_luaXml)
			return c->_luaXml;
		static const char *kDirs[] = {"Ninja", "Kuchiyose", "Kugutsu", "Guardian"};
		auto fileUtils = FileUtils::sharedFileUtils();
		const string &name = c->getName();
		for (auto dir : kDirs)
		{
			string path = fileUtils->fullPathForFilename(format("Unit/{}/{}/{}.xml", dir, name, name).c_str());
			if (!fileUtils->isFileExist(path))
				continue;
			CCArray *arr = CCArray::create();
			if (!KTools::readXMLToArray(path, arr))
				return nullptr;
			arr->retain();
			c->_luaXml = arr;
			return arr;
		}
		return nullptr;
	}

	// Aksi skillNN = urutan ke-(7 + NN). Null jika tidak ada.
	static CCArray *actionAt(CCArray *xml, int skillNN)
	{
		if (!xml || skillNN <= 0)
			return nullptr;
		const int index = 7 + skillNN;
		if ((int)xml->count() <= index)
			return nullptr;
		return (CCArray *)xml->objectAtIndex(index);
	}

	// Array frame untuk skillNN; null jika kosong / tidak ada.
	static CCArray *frames(CCArray *xml, int skillNN)
	{
		auto *action = actionAt(xml, skillNN);
		if (!action || action->count() < 2)
			return nullptr;
		CCArray *a = (CCArray *)action->objectAtIndex(1);
		return (a && a->count() > 0) ? a : nullptr;
	}

	static CCArray *baseArray(CharacterBase *c, int slot)
	{
		switch (slot)
		{
		case 1:
			return c->skill1Array;
		case 2:
			return c->skill2Array;
		case 3:
			return c->skill3Array;
		case 4:
			return c->skill4Array;
		case 5:
			return c->skill5Array;
		}
		return nullptr;
	}

	static void setSkillAction(CharacterBase *c, int slot, FiniteTimeAction *a)
	{
		switch (slot)
		{
		case 1:
			c->setSkill1Action(a);
			break;
		case 2:
			c->setSkill2Action(a);
			break;
		case 3:
			c->setSkill3Action(a);
			break;
		case 4:
			c->setSkill4Action(a);
			break;
		case 5:
			c->setSkill5Action(a);
			break;
		}
	}

	static void saveSkill(CharacterBase *c, BuffSnapshot &snap, int slot)
	{
		snap.has[slot] = true;
		switch (slot)
		{
		case 1:
			snap.type[1] = c->_sAttackType1;
			snap.value[1] = c->getSAttackValue1();
			snap.rangeX[1] = c->_sAttackRangeX1;
			snap.rangeY[1] = c->_sAttackRangeY1;
			snap.cd[1] = c->_sAttackCD1;
			break;
		case 2:
			snap.type[2] = c->_sAttackType2;
			snap.value[2] = c->getSAttackValue2();
			snap.rangeX[2] = c->_sAttackRangeX2;
			snap.rangeY[2] = c->_sAttackRangeY2;
			snap.cd[2] = c->_sAttackCD2;
			break;
		case 3:
			snap.type[3] = c->_sAttackType3;
			snap.value[3] = c->getSAttackValue3();
			snap.rangeX[3] = c->_sAttackRangeX3;
			snap.rangeY[3] = c->_sAttackRangeY3;
			snap.cd[3] = c->_sAttackCD3;
			break;
		case 4:
			snap.type[4] = c->_sAttackType4;
			snap.value[4] = c->getSAttackValue4();
			snap.rangeX[4] = c->_sAttackRangeX4;
			snap.rangeY[4] = c->_sAttackRangeY4;
			snap.cd[4] = c->_sAttackCD4;
			break;
		case 5:
			snap.type[5] = c->_sAttackType5;
			snap.value[5] = c->getSAttackValue5();
			snap.rangeX[5] = c->_sAttackRangeX5;
			snap.rangeY[5] = c->_sAttackRangeY5;
			snap.cd[5] = c->_sAttackCD5;
			break;
		}
	}

	static void writeSkill(CharacterBase *c, int slot, const string &type, uint32_t value, int rx, int ry, uint32_t cd)
	{
		switch (slot)
		{
		case 1:
			c->_sAttackType1 = type;
			c->setSAttackValue1(value);
			c->_sAttackRangeX1 = rx;
			c->_sAttackRangeY1 = ry;
			c->_sAttackCD1 = cd;
			break;
		case 2:
			c->_sAttackType2 = type;
			c->setSAttackValue2(value);
			c->_sAttackRangeX2 = rx;
			c->_sAttackRangeY2 = ry;
			c->_sAttackCD2 = cd;
			break;
		case 3:
			c->_sAttackType3 = type;
			c->setSAttackValue3(value);
			c->_sAttackRangeX3 = rx;
			c->_sAttackRangeY3 = ry;
			c->_sAttackCD3 = cd;
			break;
		case 4:
			c->_sAttackType4 = type;
			c->setSAttackValue4(value);
			c->_sAttackRangeX4 = rx;
			c->_sAttackRangeY4 = ry;
			c->_sAttackCD4 = cd;
			break;
		case 5:
			c->_sAttackType5 = type;
			c->setSAttackValue5(value);
			c->_sAttackRangeX5 = rx;
			c->_sAttackRangeY5 = ry;
			c->_sAttackCD5 = cd;
			break;
		}
	}

	// Ganti animasi idle/walk yang sedang jalan agar perubahan langsung terlihat.
	static void refreshIdle(CharacterBase *c)
	{
		if (c->isFreeState())
		{
			c->_state = State::WALK;
			c->idle();
		}
	}
};
