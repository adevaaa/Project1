#pragma once
#include "Systems/SummonConfig.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * BuffConfig
 *
 * Data + parser untuk section #Buff di lua/class/AiCompat.lua (setBuff dengan id bebas).
 * Tanpa ketergantungan ke CharacterBase. Logika game ada di Systems/BuffSystem.hpp.
 *
 *   #Buff
 *   Kabuto = { id = 11, idle = Skill10, Walk = Skill11, nAttack = Skill12, Armored, Imun, closing = timer, lock = skill02+skill03 }
 *   Zetsu  = { id = 17, idle = Skill10, Walk = Skill11, nAttack = Skill12, closing = clone }
 *
 * skillNN = aksi ke-NN di XML karakter (urutan file: skill01 = aksi ke-8, skill10 = aksi ke-17, dst).
 */
struct BuffDef
{
	std::string owner;
	int id = 0; // nilai <e type="setBuff">id</e>

	// Ganti animasi dengan aksi skillNN (0 = tidak diganti)
	int idle = 0;
	int walk = 0;
	int nattack = 0;
	int dead = 0;
	int hurt = 0;
	int skill[6] = {0, 0, 0, 0, 0, 0}; // skill01..05 diganti aksi skillNN ("skill1 = skill10" -> skill1_1)

	bool armored = false; // tidak terkena crowd control
	bool imun = false;	  // tidak terkena damage
	bool lockAll = false; // Skills = Lock: semua skill terkunci selama buff aktif

	std::vector<int> hide; // skill disembunyikan (dan dikunci) selama buff aktif
	std::vector<int> lock; // terkunci tanpa buff, terbuka selama buff aktif (seperti Kiba)

	bool closeTimer = true;	  // buff mati saat durasi habis
	bool closeResume = false; // buff mati saat setActionResume di XML dijalankan
	bool closeClone = false;  // buff mati saat semua clone/kuchiyose/kugutsu milik pemilik sudah mati (seperti Kiba)
	int endSkill = 0;		  // "timer > skillNN": jalankan aksi skillNN saat timer habis
};

// Nilai asli yang disimpan saat buff dipasang, dipulihkan saat buff mati.
struct BuffSnapshot
{
	bool armored = false;
	bool has[6] = {false, false, false, false, false, false};
	std::string type[6];
	uint32_t value[6] = {0, 0, 0, 0, 0, 0};
	int rangeX[6] = {0, 0, 0, 0, 0, 0};
	int rangeY[6] = {0, 0, 0, 0, 0, 0};
	uint32_t cd[6] = {0, 0, 0, 0, 0, 0};
};

class BuffConfig
{
public:
	static void clear() { defs().clear(); }

	static const std::vector<BuffDef> *defsFor(const std::string &owner)
	{
		auto it = defs().find(owner);
		return it == defs().end() ? nullptr : &it->second;
	}

	static const BuffDef *find(const std::string &owner, int id)
	{
		auto *list = defsFor(owner);
		if (!list)
			return nullptr;
		for (auto &d : *list)
			if (d.id == id)
				return &d;
		return nullptr;
	}

	// Isi section #Buff (boleh berbaris-baris):  Pemilik = { id = 11, idle = Skill10, Armored, ... }
	static void parse(const std::string &text)
	{
		size_t i = 0;
		const size_t n = text.size();
		while (i < n)
		{
			size_t eq = text.find('=', i);
			if (eq == std::string::npos)
				break;
			size_t brace = text.find('{', eq);
			if (brace == std::string::npos)
				break;
			if (!SummonConfig::trim(text.substr(eq + 1, brace - eq - 1)).empty())
			{
				i = eq + 1;
				continue;
			}
			const std::string owner = SummonConfig::unquote(text.substr(i, eq - i));
			int depth = 0;
			size_t end = brace;
			for (; end < n; ++end)
			{
				if (text[end] == '{')
					++depth;
				else if (text[end] == '}' && --depth == 0)
					break;
			}
			const std::string body = text.substr(brace + 1, end > brace ? end - brace - 1 : 0);
			i = end + 1;
			if (!owner.empty())
				parseBody(owner, body);
		}
	}

private:
	static std::unordered_map<std::string, std::vector<BuffDef>> &defs()
	{
		static std::unordered_map<std::string, std::vector<BuffDef>> map;
		return map;
	}

	static int firstSkill(const std::string &value)
	{
		auto list = SummonConfig::parseSkillList(value);
		return list.empty() ? 0 : list[0];
	}

	static void parseBody(const std::string &owner, const std::string &body)
	{
		// segmen dipisah koma / baris baru. Segmen tanpa '=' = flag (Armored, Imun)
		// atau lanjutan daftar sebelumnya ("hide = skill04, skill05").
		std::vector<std::pair<std::string, std::string>> items;
		std::string cur;
		auto flush = [&]()
		{
			std::string seg = SummonConfig::trim(cur);
			cur.clear();
			if (seg.empty())
				return;
			auto eq = seg.find('=');
			if (eq == std::string::npos)
			{
				std::string f = SummonConfig::lower(SummonConfig::unquote(seg));
				if (f == "armored" || f == "armor" || f == "imun" || f == "immune" || f == "invincible")
					items.push_back({f, "true"});
				else if (!items.empty())
					items.back().second += "," + seg;
				return;
			}
			items.push_back({SummonConfig::lower(SummonConfig::unquote(seg.substr(0, eq))), SummonConfig::trim(seg.substr(eq + 1))});
		};
		for (char c : body)
		{
			if (c == ',' || c == '\n')
				flush();
			else
				cur.push_back(c);
		}
		flush();

		BuffDef d;
		d.owner = owner;
		bool hasId = false;
		for (auto &kv : items)
		{
			const std::string &k = kv.first;
			const std::string &v = kv.second;
			if (k == "id")
			{
				d.id = atoi(v.c_str());
				hasId = true;
			}
			else if (k == "idle")
				d.idle = firstSkill(v);
			else if (k == "walk")
				d.walk = firstSkill(v);
			else if (k == "nattack" || k == "attack")
				d.nattack = firstSkill(v);
			else if (k == "dead")
				d.dead = firstSkill(v);
			else if (k == "hurt")
				d.hurt = firstSkill(v);
			else if (k == "armored" || k == "armor")
				d.armored = true;
			else if (k == "imun" || k == "immune" || k == "invincible")
				d.imun = true;
			else if (k == "hide")
				d.hide = SummonConfig::parseSkillList(v);
			else if (k == "lock")
				d.lock = SummonConfig::parseSkillList(v);
			else if (k == "skills")
				d.lockAll = SummonConfig::lower(SummonConfig::unquote(v)) == "lock";
			else if (k == "closing")
				parseClosing(d, v);
			else if (k.rfind("skill", 0) == 0 && k.size() > 5 && k.find_first_not_of("0123456789", 5) == std::string::npos)
			{
				int slot = atoi(k.c_str() + 5);
				if (slot >= 1 && slot <= 5)
					d.skill[slot] = firstSkill(v);
				else
					CCLOG("[AiCompat] Buff %s: %s hanya didukung skill01..skill05, dilewati", owner.c_str(), k.c_str());
			}
			else
				CCLOG("[AiCompat] Buff %s: kunci '%s' tidak dikenal, dilewati", owner.c_str(), k.c_str());
		}

		if (!hasId)
		{
			CCLOG("[AiCompat] Buff %s: id tidak ada, dilewati", owner.c_str());
			return;
		}
		defs()[owner].push_back(d);
		CCLOG("[AiCompat] Buff %s id %d (timer %d, resume %d, clone %d, endSkill %d)", owner.c_str(), d.id, d.closeTimer, d.closeResume, d.closeClone, d.endSkill);
	}

	// closing = timer | setActionResume | clone | gabungan dengan '+' ... [> skillNN]
	static void parseClosing(BuffDef &d, const std::string &value)
	{
		std::string left = value, right;
		auto gt = value.find('>');
		if (gt != std::string::npos)
		{
			left = value.substr(0, gt);
			right = value.substr(gt + 1);
		}
		bool timer = false, resume = false, clone = false;
		for (auto &t : SummonConfig::splitList(left))
		{
			if (t == "timer")
				timer = true;
			else if (t == "setactionresume" || t == "actionresume" || t == "resume")
				resume = true;
			else if (t == "clone" || t == "summon" || t == "kuchiyose" || t == "kugutsu")
				clone = true;
		}
		if (!timer && !resume && !clone)
			timer = true;
		d.closeTimer = timer;
		d.closeResume = resume;
		d.closeClone = clone;
		d.endSkill = firstSkill(right);
	}
};
