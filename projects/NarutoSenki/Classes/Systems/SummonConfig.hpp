#pragma once
#include "Utils/Cocos2dxHelper.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * SummonConfig
 *
 * Data + parser untuk section Ai Summon di lua/class/AiCompat.lua.
 * File ini TIDAK memakai CharacterBase, jadi aman di-include dari AiCompat.hpp.
 * Logika saat game berjalan ada di Systems/SummonSystem.hpp.
 *
 *   #Kugutsu            jenis unit = Kugutsu (Role::Kugutsu)
 *   SasoriET = Saso     -> SasoriET memakai kelas AI Saso
 *
 *   #Kuchiyose          jenis unit = Kuchiyose (Role::Summon)
 *   Birds = Centipede   -> Birds memakai kelas AI Centipede
 *   Rhino               -> tanpa "= AI": AI dari #Characters, jika tidak ada AI generik (LuaSummon)
 *
 *   #Ninja              jenis unit = Ninja/Clone (Role::Clone)
 *   Ashura
 *
 *   #Clone              pemilik = { UnitYangDipanggil, Id = n, ... }  (lihat AiCompat.lua)
 */

enum class SummonKind : uint8_t
{
	None,
	Ninja,
	Kuchiyose,
	Kugutsu
};

enum class SummonClosing : uint8_t
{
	Timer, // mati saat durasi XML (nilai setClone) habis
	Dead   // hidup sampai mati
};

struct SummonDef
{
	static constexpr int kAreaDefault = -1; // Area tidak diatur -> pakai bawaan AI
	static constexpr int kAreaFree = -2;	// Area = Free -> tanpa batas owner

	std::string owner; // nama karakter pemilik (getName())
	std::string unit;  // nama unit yang dipanggil
	int id = 0;
	SummonClosing closing = SummonClosing::Timer;
	int area = kAreaDefault;
	bool stride = false; // Stride: tanpa musuh, clone terus maju ke depan seperti Akamaru (balik jika terlalu jauh dari owner)

	std::vector<std::string> target;   // hanya jenis ini yang dipilih (kosong = semua)
	std::vector<std::string> untarget; // jenis ini dikecualikan

	std::vector<int> attack; // skill otomatis saat cooldown siap (1 = skill01, ...)

	struct MonAttack
	{
		int command = 1;	   // nilai setMonAttack dari XML
		std::vector<int> skills; // skill clone yang dijalankan
	};
	std::vector<MonAttack> monAttack;

	std::vector<int> lock;	 // skill owner terkunci selama clone hidup
	std::vector<int> unlock; // skill owner terkunci tanpa clone, terbuka saat clone hidup
	bool lockAll = false;	 // Skills = Lock

	// Radius area (piksel) dari owner. Area 0 sangat kecil (Parents), 1 dekat (Saso),
	// 2 sedang (Sanshouuo), 3 luas (Karasu/Akamaru), >3 melebar lagi. Free/default -> 288 (= kAttackRange).
	int radius() const
	{
		static const int kRadius[] = {48, 96, 224, 288};
		if (area < 0)
			return 288;
		return area <= 3 ? kRadius[area] : 288 + (area - 3) * 96;
	}

	bool isFree() const { return area == kAreaFree; }
	bool hasArea() const { return area >= 0; }
};

class SummonConfig
{
public:
	struct UnitDecl
	{
		SummonKind kind = SummonKind::None;
		std::string ai; // kosong = ikut #Characters / AI generik
	};

	static void clear()
	{
		units().clear();
		defs().clear();
	}

	static bool empty() { return units().empty() && defs().empty(); }

	// "SasoriET = Saso" atau "Rhino" (satu baris di section #Kugutsu/#Kuchiyose/#Ninja).
	static void declare(SummonKind kind, const std::string &line)
	{
		std::string name, ai;
		auto eq = line.find('=');
		if (eq == std::string::npos)
		{
			name = unquote(line);
		}
		else
		{
			name = unquote(line.substr(0, eq));
			ai = unquote(line.substr(eq + 1));
		}
		if (name.empty())
			return;
		auto &d = units()[name];
		d.kind = kind;
		if (!ai.empty())
			d.ai = ai;
		CCLOG("[AiCompat] Summon unit %s (jenis %d) -> AI %s", name.c_str(), (int)kind, ai.empty() ? "-" : ai.c_str());
	}

	static const UnitDecl *unit(const std::string &name)
	{
		auto it = units().find(name);
		return it == units().end() ? nullptr : &it->second;
	}

	static const std::vector<SummonDef> *defsFor(const std::string &owner)
	{
		auto it = defs().find(owner);
		return it == defs().end() ? nullptr : &it->second;
	}

	static const std::unordered_map<std::string, std::vector<SummonDef>> &allDefs() { return defs(); }

	// Pilih definisi untuk nilai <e type="setClone"> dari XML:
	//   1. Id sama persis dengan nilai XML (mis. Kabuto Id = 3 dan XML setClone 3)
	//   2. jika tidak ada, definisi ber-Id 0 (nilai XML dipakai sebagai durasi timer)
	static const SummonDef *find(const std::string &owner, int xmlValue)
	{
		auto *list = defsFor(owner);
		if (!list)
			return nullptr;
		const SummonDef *fallback = nullptr;
		for (auto &d : *list)
		{
			if (d.id == xmlValue)
				return &d;
			if (d.id == 0 && !fallback)
				fallback = &d;
		}
		return fallback;
	}

	// Isi section #Clone (boleh berbaris-baris):
	//   Kabuto = { SasoriET, Id = 3, closing = dead }
	static void parseClone(const std::string &text)
	{
		size_t i = 0;
		const size_t n = text.size();
		while (i < n)
		{
			// nama pemilik sampai '='
			size_t eq = text.find('=', i);
			if (eq == std::string::npos)
				break;
			size_t brace = text.find('{', eq);
			if (brace == std::string::npos)
				break;
			// teks antara '=' dan '{' harus kosong
			if (!trim(text.substr(eq + 1, brace - eq - 1)).empty())
			{
				i = eq + 1;
				continue;
			}
			const std::string owner = unquote(text.substr(i, eq - i));

			int depth = 0;
			size_t end = brace;
			for (; end < n; ++end)
			{
				if (text[end] == '{')
					++depth;
				else if (text[end] == '}' && --depth == 0)
					break;
			}
			std::string body = text.substr(brace + 1, end > brace ? end - brace - 1 : 0);
			i = end + 1;
			if (!owner.empty())
				parseBody(owner, body);
		}
	}

	// "skill02+skill03", "Skill1, skill2", "skill09" -> {2,3} / {1,2} / {9}
	static std::vector<int> parseSkillList(const std::string &value)
	{
		std::vector<int> out;
		for (auto &tok : splitList(value))
		{
			std::string t = tok;
			if (t.rfind("skill", 0) == 0)
				t = t.substr(5);
			else if (t.rfind("s", 0) == 0 && t.size() > 1 && isdigit((unsigned char)t[1]))
				t = t.substr(1);
			if (t.empty() || t.find_first_not_of("0123456789") != std::string::npos)
				continue;
			int v = atoi(t.c_str());
			if (v > 0 && std::find(out.begin(), out.end(), v) == out.end())
				out.push_back(v);
		}
		return out;
	}

	// Daftar nilai dipisah koma, +, |, atau ;  (huruf kecil, tanpa kutip)
	static std::vector<std::string> splitList(const std::string &value)
	{
		std::vector<std::string> out;
		std::string cur;
		auto flush = [&]()
		{
			std::string t = lower(unquote(cur));
			if (!t.empty())
				out.push_back(t);
			cur.clear();
		};
		for (char c : value)
		{
			if (c == ',' || c == '+' || c == '|' || c == ';')
				flush();
			else
				cur.push_back(c);
		}
		flush();
		return out;
	}

	static std::string trim(const std::string &s)
	{
		const char *ws = " \t\r\n";
		auto b = s.find_first_not_of(ws);
		if (b == std::string::npos)
			return "";
		auto e = s.find_last_not_of(ws);
		return s.substr(b, e - b + 1);
	}

	static std::string lower(std::string s)
	{
		for (auto &c : s)
			c = (char)tolower((unsigned char)c);
		return s;
	}

	static std::string unquote(std::string v)
	{
		v = trim(v);
		while (!v.empty() && (v.front() == '\'' || v.front() == '"'))
			v.erase(v.begin());
		while (!v.empty() && (v.back() == '\'' || v.back() == '"'))
			v.pop_back();
		return trim(v);
	}

private:
	static std::unordered_map<std::string, UnitDecl> &units()
	{
		static std::unordered_map<std::string, UnitDecl> map;
		return map;
	}

	static std::unordered_map<std::string, std::vector<SummonDef>> &defs()
	{
		static std::unordered_map<std::string, std::vector<SummonDef>> map;
		return map;
	}

	static void parseBody(const std::string &owner, const std::string &body)
	{
		// Pecah menurut koma / baris baru. Segmen tanpa '=' setelah kunci
		// (mis. "untarget = shinobi, guardian") menyambung ke daftar kunci sebelumnya.
		std::vector<std::pair<std::string, std::string>> items; // kunci (kosong = nama unit), nilai
		std::string cur;
		auto flush = [&]()
		{
			std::string seg = trim(cur);
			cur.clear();
			if (seg.empty())
				return;
			auto eq = seg.find('=');
			if (eq == std::string::npos)
			{
				// Kunci tanpa nilai: "Stride" (sama dengan "Stride = true")
				if (!items.empty() && lower(unquote(seg)) == "stride")
				{
					items.push_back({"stride", "true"});
					return;
				}
				if (items.empty())
					items.push_back({"", unquote(seg)}); // nama unit
				else
					items.back().second += "," + seg;	  // lanjutan daftar
				return;
			}
			items.push_back({lower(unquote(seg.substr(0, eq))), trim(seg.substr(eq + 1))});
		};
		for (char c : body)
		{
			if (c == ',' || c == '\n')
				flush();
			else
				cur.push_back(c);
		}
		flush();

		SummonDef def;
		def.owner = owner;
		bool hasUnit = false;
		for (auto &kv : items)
		{
			const std::string &k = kv.first;
			const std::string &v = kv.second;
			if (k.empty())
			{
				if (!hasUnit)
				{
					def.unit = unquote(v);
					hasUnit = true;
				}
			}
			else if (k == "id")
				def.id = atoi(v.c_str());
			else if (k == "closing")
			{
				std::string t = lower(unquote(v));
				def.closing = (t == "dead" || t == "death" || t == "die") ? SummonClosing::Dead : SummonClosing::Timer;
			}
			else if (k == "area")
			{
				std::string t = lower(unquote(v));
				if (t == "free")
					def.area = SummonDef::kAreaFree;
				else if (!t.empty() && t.find_first_not_of("0123456789") == std::string::npos)
					def.area = atoi(t.c_str());
			}
			else if (k == "stride")
			{
				std::string t = lower(unquote(v));
				def.stride = !(t == "false" || t == "0" || t == "no");
			}
			else if (k == "target")
				def.target = splitList(v);
			else if (k == "untarget")
				def.untarget = splitList(v);
			else if (k == "attack")
				def.attack = parseSkillList(v);
			else if (k == "setmonattack")
			{
				// "1 = skill01"  atau  "1 = skill01+skill02"
				SummonDef::MonAttack ma;
				auto eq = v.find('=');
				if (eq == std::string::npos)
					ma.skills = parseSkillList(v);
				else
				{
					ma.command = atoi(trim(v.substr(0, eq)).c_str());
					ma.skills = parseSkillList(v.substr(eq + 1));
				}
				if (ma.command > 0 && !ma.skills.empty())
					def.monAttack.push_back(ma);
			}
			else if (k == "lock")
				def.lock = parseSkillList(v);
			else if (k == "unlock")
				def.unlock = parseSkillList(v);
			else if (k == "skills")
				def.lockAll = lower(unquote(v)) == "lock";
			else
				CCLOG("[AiCompat] Clone %s: kunci '%s' tidak dikenal, dilewati", owner.c_str(), k.c_str());
		}

		if (def.unit.empty())
		{
			CCLOG("[AiCompat] Clone %s: nama unit kosong, dilewati", owner.c_str());
			return;
		}
		defs()[owner].push_back(def);
		CCLOG("[AiCompat] Clone %s -> %s (Id %d, closing %s, area %d)", owner.c_str(), def.unit.c_str(), def.id,
			  def.closing == SummonClosing::Dead ? "dead" : "timer", def.area);
	}
};
