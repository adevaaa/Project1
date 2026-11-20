#pragma once
#include "Core/Provider.hpp"
#include "GameLayer.h"
#include "HudLayer.h"
#include "Systems/AiCompat.hpp"

/**
 * SkillLock
 *
 * Satu tempat untuk kunci/sembunyi skill milik pemilik, dari dua sumber di AiCompat.lua:
 *   #Clone : lock / unlock / Skills = Lock       (selama summon hidup)
 *   #Buff  : lock / Skills = Lock / Hide         (selama buff aktif)
 * Hasilnya disimpan di CharacterBase::_luaSkillLock[1..5] dan _luaSkillHide[1..5].
 * CharacterBase::attack menolak skill yang terkunci; tombol HUD pemain ikut dikunci/disembunyikan.
 */
class SkillLock
{
public:
	static void recompute(CharacterBase *owner)
	{
		bool want[6] = {false, false, false, false, false, false};
		bool hide[6] = {false, false, false, false, false, false};

		// ---- Summon (#Clone) ----
		const auto *sdefs = AiCompat::summonDefs(owner->getName());
		if (sdefs)
			for (auto &d : *sdefs)
				for (int s : d.unlock)
					if (s >= 1 && s <= 5)
						want[s] = true; // terkunci tanpa clone

		const auto live = owner->_luaSummons;
		for (auto &e : live)
		{
			if (!e.second)
				continue;
			if (e.second->lockAll)
				for (int s = 1; s <= 5; ++s)
					want[s] = true;
			for (int s : e.second->lock)
				if (s >= 1 && s <= 5)
					want[s] = true;
		}
		for (auto &e : live)
		{
			if (!e.second)
				continue;
			for (int s : e.second->unlock)
				if (s >= 1 && s <= 5)
					want[s] = false; // clone hidup -> terbuka
		}

		// ---- Buff (#Buff) ----
		const auto *bdefs = AiCompat::buffDefs(owner->getName());
		if (bdefs)
			for (auto &d : *bdefs)
				for (int s : d.lock)
					if (s >= 1 && s <= 5)
						want[s] = true; // terkunci tanpa buff
		if (owner->_luaBuff)
		{
			const BuffDef &b = *owner->_luaBuff;
			if (b.lockAll)
				for (int s = 1; s <= 5; ++s)
					want[s] = true;
			for (int s : b.lock)
				if (s >= 1 && s <= 5)
					want[s] = false; // buff aktif -> terbuka
			for (int s : b.hide)
				if (s >= 1 && s <= 5)
				{
					want[s] = true;
					hide[s] = true;
				}
		}

		for (int s = 1; s <= 5; ++s)
		{
			if (owner->_luaSkillLock[s] == want[s] && owner->_luaSkillHide[s] == hide[s])
				continue;
			owner->_luaSkillLock[s] = want[s];
			owner->_luaSkillHide[s] = hide[s];
			updateButton(owner, s);
		}
	}

	// Dipanggil Provider::create: pasang kunci awal untuk pemilik yang punya unlock (#Clone) atau lock (#Buff).
	static void applyBaseline(CharacterBase *owner)
	{
		if (!AiCompat::summonDefs(owner->getName()) && !AiCompat::buffDefs(owner->getName()))
			return;
		recompute(owner);
		// Tombol HUD belum ada saat karakter dibuat -> sinkronkan lagi setelah HUD siap.
		auto layer = getGameLayer();
		if (layer && !layer->isHUDInit())
		{
			RefPtr<CharacterBase> keep(owner);
			layer->onHUDInitialized([keep]()
									{ sync(keep.get()); });
		}
	}

	static void sync(CharacterBase *owner)
	{
		for (int s = 1; s <= 5; ++s)
			updateButton(owner, s);
	}

private:
	// Tombol HUD hanya untuk pemain; COM dicegah oleh CharacterBase::attack.
	static void updateButton(CharacterBase *owner, int skill)
	{
		auto layer = getGameLayer();
		if (!owner->isPlayer() || !layer || !layer->isHUDInit() || !layer->getHudLayer())
			return;
		auto hud = layer->getHudLayer();
		ActionButton *btn = nullptr;
		switch (skill)
		{
		case 1:
			btn = hud->skill1Button;
			break;
		case 2:
			btn = hud->skill2Button;
			break;
		case 3:
			btn = hud->skill3Button;
			break;
		case 4:
			btn = hud->skill4Button;
			break;
		case 5:
			btn = hud->skill5Button;
			break;
		}
		if (!btn)
			return;
		btn->setVisible(!owner->_luaSkillHide[skill]);
		if (owner->_luaSkillLock[skill])
			btn->setLock();
		else
			btn->unLock();
	}
};
