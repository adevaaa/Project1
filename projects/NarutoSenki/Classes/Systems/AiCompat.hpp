#pragma once
#include "Utils/Cocos2dxHelper.hpp"
#include "Systems/BuffConfig.hpp"
#include "Systems/SummonConfig.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * AiCompat
 *
 * Membaca lua/class/AiCompat.lua (format teks dengan section '#')
 * dan menyediakan alias: nama mon -> nama AI (CloneAI) yang dipakai
 * oleh CharacterBase::setMon.
 *
 * Contoh isi file:
 *
 *   #Monster
 *   Chibaku = JibakuEX
 *
 *   #MainChar
 *   'Asuma', 'Chiyo', 'Choji'
 *
 *   #Bullets
 *   RinSRK = TentenSRK
 *
 *   #Characters
 *   Madara = Ino
 *   Konohamaru = {Hiruzen, Skill1 = Tobi, skill2 = Kimimaro}
 *
 * -> AiCompat::resolveMon("Chibaku") == "JibakuEX"
 * -> AiCompat::mainChars() == {"Asuma", "Chiyo", "Choji"}
 *   #Traps
 *   Amaterasu2 = Amaterasu
 *
 *   #Bullets
 *   Shinteshin2 = Shintenshin, InoWar    (opsional: ", Master" = hanya untuk InoWar)
 *
 *   #Monster
 *   Kage2 = Kage, Shikaku, KageHand2     (opsional: ", Tautan" = mon yang dikeluarkan Kage2)
 *
 *   #Transform
 *   Guy = Guy8Gates
 *
 *   #Load/UnloadChar
 *   Guy = { ninja = 'Guy8Gates', skill = 'Guy8Gates' }
 *
 * -> AiCompat::resolveTrap("Amaterasu2") == "Amaterasu"
 * -> AiCompat::transformTarget("Guy") == "Guy8Gates"
 * -> AiCompat::plistConfig("Guy") == {{"ninja","Guy8Gates"},{"skill","Guy8Gates"}}
 * -> AiCompat::resolveBullet("RinSRK") == "TentenSRK"
 * -> AiCompat::resolveChar("Madara") == "Ino"
 * -> AiCompat::charRanges("Konohamaru") == {{"skill1","Tobi"},{"skill2","Kimimaro"}}
 */
class AiCompat
{
public:
	static constexpr const char *kFilePath = "class/AiCompat.lua";

	// Nama AI untuk monName. Jika tidak ada alias, mengembalikan monName itu sendiri.
	static std::string resolveMon(const std::string &monName)
	{
		auto &map = monsterMap();
		auto it = map.find(monName);
		if (it == map.end())
			return monName;

		// Ikuti rantai alias (A = B, B = C), dengan batas supaya tidak loop tak berujung.
		std::string cur = it->second;
		for (int depth = 0; depth < 8; ++depth)
		{
			auto next = map.find(cur);
			if (next == map.end() || next->second == cur)
				return cur;
			cur = next->second;
		}
		return cur;
	}

	static bool hasMonAlias(const std::string &monName)
	{
		return monsterMap().find(monName) != monsterMap().end();
	}

	// true jika Resources/Unit/Mon/<monName>.xml ada (tanpa memunculkan pesan error).
	static bool monXmlExists(const std::string &monName)
	{
		std::string path = "Unit/Mon/" + monName + ".xml";
		unsigned long size = 0;
		unsigned char *data = FileUtils::sharedFileUtils()->getFileData(path.c_str(), "r", &size);
		if (!data)
			return false;
		delete[] data;
		return true;
	}

	// Nama bullet yang dipakai untuk perilaku/AI (section #Bullets). Tanpa alias -> `bulletName` itu sendiri.
	static std::string resolveBullet(const std::string &bulletName)
	{
		auto &map = bulletMap();
		auto it = map.find(bulletName);
		if (it == map.end())
			return bulletName;

		std::string cur = it->second;
		for (int depth = 0; depth < 8; ++depth)
		{
			auto next = map.find(cur);
			if (next == map.end() || next->second == cur)
				return cur;
			cur = next->second;
		}
		return cur;
	}

	// Nama trap yang dipakai untuk perilaku (section #Traps): Amaterasu2 = Amaterasu -> "Amaterasu".
	// Tanpa alias -> `trapName` itu sendiri.
	static std::string resolveTrap(const std::string &trapName)
	{
		auto &map = trapMap();
		auto it = map.find(trapName);
		if (it == map.end())
			return trapName;

		std::string cur = it->second;
		for (int depth = 0; depth < 8; ++depth)
		{
			auto next = map.find(cur);
			if (next == map.end() || next->second == cur)
				return cur;
			cur = next->second;
		}
		return cur;
	}

	// true jika data trap ada: Unit/Mon/<nama>.xml (trap monster) atau Unit/Projectile/<nama>.xml (trap bullet).
	static bool trapXmlExists(const std::string &trapName, bool isMonster)
	{
		return isMonster ? monXmlExists(trapName) : projectileXmlExists(trapName);
	}

	// true jika `name` berperilaku sebagai `base`, baik namanya sendiri maupun lewat alias
	// #Traps / #Bullets / #Monster. Dipakai untuk pengecekan yang tadinya terkunci ke nama asli
	// (mis. efek KageBom, frame mati Amaterasu).
	static bool behaviorIs(const std::string &name, const char *base)
	{
		if (name == base)
			return true;
		if (!hasAliases())
			return false;
		return resolveTrap(name) == base || resolveBullet(name) == base || resolveMon(name) == base;
	}

	// true jika alias `name` boleh dipakai oleh pembuatnya. Alias tanpa master berlaku untuk semua;
	// alias dengan master ("Nama = Perilaku, Master") hanya berlaku jika pembuatnya (atau master si
	// pembuat, mis. untuk clone) bernama Master.
	static bool aliasAppliesTo(const std::string &name, const std::string &creator, const std::string &creatorMaster)
	{
		ensureLoaded();
		auto &masters = rawAliasMaster();
		auto it = masters.find(name);
		if (it == masters.end())
			return true;
		return it->second == creator || (!creatorMaster.empty() && it->second == creatorMaster);
	}

	static std::string resolveMonFor(const std::string &name, const std::string &creator, const std::string &creatorMaster = "")
	{
		return aliasAppliesTo(name, creator, creatorMaster) ? resolveMon(name) : name;
	}

	static std::string resolveBulletFor(const std::string &name, const std::string &creator, const std::string &creatorMaster = "")
	{
		return aliasAppliesTo(name, creator, creatorMaster) ? resolveBullet(name) : name;
	}

	static std::string resolveTrapFor(const std::string &name, const std::string &creator, const std::string &creatorMaster = "")
	{
		return aliasAppliesTo(name, creator, creatorMaster) ? resolveTrap(name) : name;
	}

	// Mon yang dikeluarkan `name` (bagian ketiga alias). Jika tidak diatur, `defaultName` dipakai.
	// Contoh: Monster::setAI Kage memanggil setMon(linkedMon(getName(), "KageHand")).
	static std::string linkedMon(const std::string &name, const char *defaultName)
	{
		ensureLoaded();
		auto &links = rawAliasLink();
		auto it = links.find(name);
		return it == links.end() ? std::string(defaultName) : it->second;
	}

	// Pengganti `master->getName() == HeroEnum::X` untuk mon/bullet `mobName`.
	// true jika master memang bernama heroName, ATAU mon ini adalah alias yang terikat ke master tersebut
	// dan perilaku aslinya milik heroName (mis. IndraSusano = ItachiSusano, Indra -> dianggap milik Itachi).
	static bool masterIs(const std::string &masterName, const char *heroName, const std::string &mobName)
	{
		if (masterName == heroName)
			return true;
		if (!hasAliases())
			return false;
		auto &masters = rawAliasMaster();
		auto it = masters.find(mobName);
		if (it == masters.end() || it->second != masterName)
			return false;
		return behaviorOwner(behaviorOf(mobName)) == std::string(heroName);
	}

	// true jika Resources/Unit/Projectile/<bulletName>.xml ada.
	static bool projectileXmlExists(const std::string &bulletName)
	{
		std::string path = "Unit/Projectile/" + bulletName + ".xml";
		unsigned long size = 0;
		unsigned char *data = FileUtils::sharedFileUtils()->getFileData(path.c_str(), "r", &size);
		if (!data)
			return false;
		delete[] data;
		return true;
	}

	// Bentuk transform untuk `name` (section #Transform), mis. Guy -> Guy8Gates. Kosong jika tidak ada.
	static std::string transformTarget(const std::string &name)
	{
		auto &map = transformMap();
		auto it = map.find(name);
		return it == map.end() ? std::string() : it->second;
	}

	// Rantai bentuk setelah `name`: Guy -> {Guy8Gates, ...}. Tanpa duplikat, tanpa `name` sendiri.
	static std::vector<std::string> transformForward(const std::string &name)
	{
		std::vector<std::string> result;
		std::string cur = name;
		for (int depth = 0; depth < 8; ++depth)
		{
			std::string next = transformTarget(cur);
			if (next.empty() || next == name || contains(result, next))
				break;
			result.push_back(next);
			cur = next;
		}
		return result;
	}

	// Semua bentuk yang terhubung dengan `name` lewat #Transform (maju maupun mundur), tanpa `name` sendiri.
	// Dipakai saat unload supaya aset bentuk asal/tujuan ikut dilepas.
	static std::vector<std::string> transformGroup(const std::string &name)
	{
		std::vector<std::string> result;
		std::vector<std::string> queue{name};
		auto &map = transformMap();
		for (size_t i = 0; i < queue.size() && queue.size() < 32; ++i)
		{
			const std::string cur = queue[i];
			for (auto &kv : map)
			{
				std::string other;
				if (kv.first == cur)
					other = kv.second;
				else if (kv.second == cur)
					other = kv.first;
				if (!other.empty() && other != name && !contains(queue, other))
				{
					queue.push_back(other);
					result.push_back(other);
				}
			}
		}
		return result;
	}

	// Daftar {jenis, nama} plist tambahan untuk karakter `name` (section #Load/UnloadChar).
	// Jenis: ninja, skill, skill2.., kuchiyose, kugutsu, guardian.
	static const std::vector<std::pair<std::string, std::string>> &plistConfig(const std::string &name)
	{
		static const std::vector<std::pair<std::string, std::string>> empty;
		ensureLoaded();
		auto &map = rawPlistConfig();
		auto it = map.find(name);
		return it == map.end() ? empty : it->second;
	}

	// Path plist untuk satu entri konfigurasi. Kosong jika jenisnya tidak dikenal.
	static std::string plistPath(const std::string &kind, const std::string &n)
	{
		if (kind == "ninja")
			return "Unit/Ninja/" + n + "/" + n + ".plist";
		if (kind == "skill")
			return "Unit/Ninja/" + n + "/" + n + "_Skill.plist";
		if (kind.size() > 5 && kind.compare(0, 5, "skill") == 0)
			return "Unit/Ninja/" + n + "/" + n + "_Skill" + kind.substr(5) + ".plist";
		if (kind == "kuchiyose")
			return "Unit/Kuchiyose/" + n + "/" + n + ".plist";
		if (kind == "kugutsu")
			return "Unit/Kugutsu/" + n + "/" + n + ".plist";
		if (kind == "guardian")
			return "Unit/Guardian/" + n + "/" + n + ".plist";
		return "";
	}

	// Nama kelas AI untuk karakter `name` (section #Char). Tanpa alias -> `name` itu sendiri.
	static std::string resolveChar(const std::string &name)
	{
		auto &map = charMap();
		auto it = map.find(name);
		if (it == map.end() || it->second.ai.empty())
			return name;

		std::string cur = it->second.ai;
		for (int depth = 0; depth < 8; ++depth)
		{
			auto next = map.find(cur);
			if (next == map.end() || next->second.ai.empty() || next->second.ai == cur)
				return cur;
			cur = next->second.ai;
		}
		return cur;
	}

	static bool hasCharAlias(const std::string &name)
	{
		return charMap().find(name) != charMap().end();
	}

	// Daftar {kunci, donor} jarak serangan untuk karakter `name`.
	// Kunci: nattack, skill1..skill5, spc1..spc3. Kosong jika tidak ada.
	static const std::vector<std::pair<std::string, std::string>> &charRanges(const std::string &name)
	{
		static const std::vector<std::pair<std::string, std::string>> empty;
		auto &map = charMap();
		auto it = map.find(name);
		return it == map.end() ? empty : it->second.ranges;
	}

	// true jika data unit ada di salah satu Unit/{Ninja,Kuchiyose,Kugutsu,Guardian}/<name>/<name>.xml.
	static bool unitXmlExists(const std::string &name)
	{
		static const char *kDirs[] = {"Ninja", "Kuchiyose", "Kugutsu", "Guardian"};
		for (auto dir : kDirs)
		{
			std::string path = std::string("Unit/") + dir + "/" + name + "/" + name + ".xml";
			unsigned long size = 0;
			unsigned char *data = FileUtils::sharedFileUtils()->getFileData(path.c_str(), "r", &size);
			if (data)
			{
				delete[] data;
				return true;
			}
		}
		return false;
	}

	// true jika Resources/Unit/Ninja/<name>/<name>.xml ada (data karakter utama).
	static bool heroXmlExists(const std::string &name)
	{
		std::string path = "Unit/Ninja/" + name + "/" + name + ".xml";
		unsigned long size = 0;
		unsigned char *data = FileUtils::sharedFileUtils()->getFileData(path.c_str(), "r", &size);
		if (!data)
			return false;
		delete[] data;
		return true;
	}

	// Daftar nama dari section #MainChar (urutan sesuai file, tanpa duplikat).
	// Kosong jika section tidak ada.
	static const std::vector<std::string> &mainChars()
	{
		ensureLoaded();
		return rawMainChars();
	}

	// ---- Ai Summon (#Kugutsu / #Kuchiyose / #Ninja / #Clone) ----

	// Jenis unit yang dideklarasikan di #Kugutsu/#Kuchiyose/#Ninja. None jika tidak dideklarasikan.
	static SummonKind summonKind(const std::string &name)
	{
		ensureLoaded();
		auto *d = SummonConfig::unit(name);
		return d ? d->kind : SummonKind::None;
	}

	// Nama kelas AI untuk unit summon `name`: "Birds = Centipede" -> "Centipede".
	// Tanpa "= AI" dipakai #Characters (mis. Rhino = NarakaPath), lalu `name` sendiri.
	static std::string resolveSummonAI(const std::string &name)
	{
		ensureLoaded();
		auto *d = SummonConfig::unit(name);
		if (d && !d->ai.empty())
		{
			// ikuti rantai (A = B, B = C) lewat #Characters juga
			std::string cur = d->ai;
			for (int depth = 0; depth < 8; ++depth)
			{
				auto *next = SummonConfig::unit(cur);
				if (next && !next->ai.empty() && next->ai != cur)
					cur = next->ai;
				else
					break;
			}
			return resolveChar(cur);
		}
		return resolveChar(name);
	}

	// Definisi Ai Summon untuk pemilik `owner` dan nilai <e type="setClone"> dari XML.
	static const SummonDef *summonDef(const std::string &owner, int xmlValue)
	{
		ensureLoaded();
		return SummonConfig::find(owner, xmlValue);
	}

	static const std::vector<SummonDef> *summonDefs(const std::string &owner)
	{
		ensureLoaded();
		return SummonConfig::defsFor(owner);
	}

	// ---- #Buff (setBuff dengan id bebas) ----
	static const BuffDef *buffDef(const std::string &owner, int id)
	{
		ensureLoaded();
		return BuffConfig::find(owner, id);
	}

	static const std::vector<BuffDef> *buffDefs(const std::string &owner)
	{
		ensureLoaded();
		return BuffConfig::defsFor(owner);
	}

	// ---- #Catch (reaksi tangkap ct_hit meminjam unit lain) ----

	// Donor perilaku ct_hit untuk penyerang `name` (atau alias-nya). Kosong jika tidak diatur.
	// "Rashomon = SandBall" -> catchDonor("Rocket", "Rashomon") == "SandBall"
	static std::string catchDonor(const std::string &name, const std::string &alias)
	{
		ensureLoaded();
		auto &map = rawCatchMap();
		if (map.empty())
			return std::string();
		auto it = map.find(alias);
		if (it == map.end() && alias != name)
			it = map.find(name);
		return it == map.end() ? std::string() : it->second;
	}

	// Donor bergaya hero: reaksinya ada di cabang "penyerang tanpa master" (Lee, Kakuzu, Tobi).
	static bool catchIsHeroStyle(const std::string &donor)
	{
		return donor == "Lee" || donor == "RockLee" || donor == "Kakuzu" || donor == "Tobi";
	}

	// Pemilik asli perilaku donor yang reaksinya bergantung pada nama master (Shikamaru, Itachi, Chiyo, Nagato).
	static std::string catchOwner(const std::string &donor)
	{
		static const std::unordered_map<std::string, std::string> owners = {
			{"Kage", "Shikamaru"},
			{"KageHand", "Shikamaru"},
			{"KageBom", "Shikamaru"},
			{"KageFeng", "Shikamaru"},
			{"QuanRen", "Shikamaru"},
			{"Tsukuyomi", "Itachi"},
			{"Amaterasu", "Itachi"},
			{"ItachiSusano", "Itachi"},
			{"Parents", "Chiyo"},
			{"NarakaPath", "Nagato"},
		};
		auto it = owners.find(donor);
		return it == owners.end() ? std::string() : it->second;
	}

	// Baca ulang file (mis. setelah file diubah saat debug).
	// Catatan: HeroRoster dibangun sekali saat pertama dipakai dan tidak ikut dibaca ulang.
	static void reload()
	{
		rawMonsterMap().clear();
		rawBulletMap().clear();
		rawTrapMap().clear();
		rawAliasMaster().clear();
		rawAliasLink().clear();
		rawTransformMap().clear();
		rawPlistConfig().clear();
		rawMainChars().clear();
		rawCharMap().clear();
		SummonConfig::clear();
		BuffConfig::clear();
		rawCatchMap().clear();
		loaded() = false;
		ensureLoaded();
	}

private:
	static std::unordered_map<std::string, std::string> &rawCatchMap()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	static std::unordered_map<std::string, std::string> &monsterMap()
	{
		ensureLoaded();
		return rawMonsterMap();
	}

	static std::unordered_map<std::string, std::string> &rawMonsterMap()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	static bool contains(const std::vector<std::string> &list, const std::string &v)
	{
		for (auto &e : list)
			if (e == v)
				return true;
		return false;
	}

	// Nama mon/bullet/trap -> master yang memilikinya (bagian kedua dari "Nama = Perilaku, Master").
	static std::unordered_map<std::string, std::string> &rawAliasMaster()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	// "Nama = Perilaku", "Nama = Perilaku, Master", atau "Nama = Perilaku, Master, Tautan".
	// Tautan = mon yang dikeluarkan mon ini (mis. Kage2 = Kage, Shikaku, KageHand2: Kage2 menyambung ke KageHand2).
	// Bagian Master boleh dikosongkan: "Nama = Perilaku, , Tautan".
	static void parseAlias(std::string key, std::string val, std::unordered_map<std::string, std::string> &map, const char *label)
	{
		key = unquote(key);
		std::vector<std::string> parts;
		size_t start = 0;
		while (parts.size() < 3)
		{
			auto comma = val.find(',', start);
			if (comma == std::string::npos)
			{
				parts.push_back(unquote(val.substr(start)));
				break;
			}
			parts.push_back(unquote(val.substr(start, comma - start)));
			start = comma + 1;
		}
		std::string behavior = parts.size() > 0 ? parts[0] : "";
		std::string master = parts.size() > 1 ? parts[1] : "";
		std::string link = parts.size() > 2 ? parts[2] : "";
		if (key.empty() || behavior.empty())
			return;

		map[key] = behavior;
		if (!master.empty())
			rawAliasMaster()[key] = master;
		if (!link.empty())
		{
			rawAliasLink()[key] = link;
			// Tautan ikut terikat ke master yang sama (kecuali sudah punya master sendiri).
			if (!master.empty() && rawAliasMaster().find(link) == rawAliasMaster().end())
				rawAliasMaster()[link] = master;
		}
		CCLOG("[AiCompat] %s %s -> AI %s (master: %s, tautan: %s)", label, key.c_str(), behavior.c_str(),
			  master.empty() ? "-" : master.c_str(), link.empty() ? "-" : link.c_str());
	}

	// Nama mon -> mon yang dikeluarkan/disambungkannya.
	static std::unordered_map<std::string, std::string> &rawAliasLink()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	static bool hasAliases()
	{
		ensureLoaded();
		return !rawMonsterMap().empty() || !rawBulletMap().empty() || !rawTrapMap().empty();
	}

	// Nama perilaku (setelah alias) untuk mon/bullet/trap `name`; `name` sendiri jika bukan alias.
	static std::string behaviorOf(const std::string &name)
	{
		std::string b = resolveMon(name);
		if (b != name)
			return b;
		b = resolveBullet(name);
		if (b != name)
			return b;
		return resolveTrap(name);
	}

	// Karakter pemilik asli perilaku bawaan (dipakai agar pengecekan "master-nya Shikamaru/Itachi"
	// tetap berlaku untuk mon/bullet alias milik master lain).
	static const char *behaviorOwner(const std::string &behavior)
	{
		static const std::unordered_map<std::string, const char *> owners = {
			{"Kage", "Shikamaru"},
			{"KageHand", "Shikamaru"},
			{"KageBom", "Shikamaru"},
			{"KageFeng", "Shikamaru"},
			{"QuanRen", "Shikamaru"},
			{"ItachiSusano", "Itachi"},
			{"Tsukuyomi", "Itachi"},
			{"Amaterasu", "Itachi"},
		};
		auto it = owners.find(behavior);
		return it == owners.end() ? "" : it->second;
	}

	static std::unordered_map<std::string, std::string> &trapMap()
	{
		ensureLoaded();
		return rawTrapMap();
	}

	static std::unordered_map<std::string, std::string> &rawTrapMap()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	static std::unordered_map<std::string, std::string> &transformMap()
	{
		ensureLoaded();
		return rawTransformMap();
	}

	static std::unordered_map<std::string, std::string> &rawTransformMap()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> &rawPlistConfig()
	{
		static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> map;
		return map;
	}

	static bool isCharSection(const std::string &section)
	{
		return section == "char" || section == "chars" || section == "character" || section == "characters";
	}

	static bool isLoadSection(const std::string &section)
	{
		return section == "load/unloadchar" || section == "loadchar" || section == "unloadchar" ||
			   section == "load/unload" || section == "loadunloadchar";
	}

	static bool isPlistKind(const std::string &key)
	{
		if (key == "ninja" || key == "skill" || key == "kuchiyose" || key == "kugutsu" || key == "guardian")
			return true;
		if (key.size() > 5 && key.compare(0, 5, "skill") == 0)
		{
			for (size_t i = 5; i < key.size(); ++i)
				if (!isdigit((unsigned char)key[i]))
					return false;
			return true;
		}
		return false;
	}

	// Isi section #Load/UnloadChar:
	//   Guy = { ninja = 'Guy8Gates', skill = 'Guy8Gates', skill2 = 'Guy8Gates', },
	//   Zetsu = { ninja = 'Zetsu2', Kuchiyose = 'WhiteZetsu' },
	// Baris boleh berbaris-baris, koma penutup boleh ada, nilai boleh '...' atau {'A', 'B'}.
	static void parseLoadConfig(const std::string &text)
	{
		struct Tok
		{
			char type; // 'w' kata/string, atau salah satu dari = { } ,
			std::string text;
		};
		std::vector<Tok> toks;
		for (size_t i = 0; i < text.size();)
		{
			char c = text[i];
			if (c == '\'' || c == '"')
			{
				size_t j = text.find(c, i + 1);
				if (j == std::string::npos)
					j = text.size();
				toks.push_back({'w', trim(text.substr(i + 1, j - i - 1))});
				i = j + 1;
			}
			else if (c == '=' || c == '{' || c == '}' || c == ',')
			{
				toks.push_back({c, ""});
				++i;
			}
			else if (isalnum((unsigned char)c) || c == '_')
			{
				size_t j = i;
				while (j < text.size() && (isalnum((unsigned char)text[j]) || text[j] == '_'))
					++j;
				toks.push_back({'w', text.substr(i, j - i)});
				i = j;
			}
			else
				++i;
		}

		size_t i = 0;
		const size_t n = toks.size();
		while (i < n)
		{
			if (!(toks[i].type == 'w' && i + 2 < n && toks[i + 1].type == '=' && toks[i + 2].type == '{'))
			{
				++i;
				continue;
			}
			const std::string name = toks[i].text;
			i += 3;
			auto &entries = rawPlistConfig()[name];
			while (i < n && toks[i].type != '}')
			{
				if (!(toks[i].type == 'w' && i + 1 < n && toks[i + 1].type == '='))
				{
					++i;
					continue;
				}
				const std::string key = lower(toks[i].text);
				i += 2;
				std::vector<std::string> values;
				if (i < n && toks[i].type == '{')
				{
					++i;
					while (i < n && toks[i].type != '}')
					{
						if (toks[i].type == 'w')
							values.push_back(toks[i].text);
						++i;
					}
					if (i < n)
						++i; // '}'
				}
				else if (i < n && toks[i].type == 'w')
				{
					values.push_back(toks[i].text);
					++i;
				}
				if (!isPlistKind(key))
				{
					CCLOG("[AiCompat] %s: jenis plist '%s' tidak dikenal, dilewati", name.c_str(), key.c_str());
					continue;
				}
				for (auto &v : values)
				{
					if (v.empty())
						continue;
					bool dup = false;
					for (auto &e : entries)
						if (e.first == key && e.second == v)
							dup = true;
					if (!dup)
					{
						entries.push_back({key, v});
						CCLOG("[AiCompat] Load %s: %s = %s", name.c_str(), key.c_str(), v.c_str());
					}
				}
			}
			if (i < n)
				++i; // '}'
		}
	}

	static std::unordered_map<std::string, std::string> &bulletMap()
	{
		ensureLoaded();
		return rawBulletMap();
	}

	static std::unordered_map<std::string, std::string> &rawBulletMap()
	{
		static std::unordered_map<std::string, std::string> map;
		return map;
	}

	struct CharAlias
	{
		std::string ai;
		std::vector<std::pair<std::string, std::string>> ranges;
	};

	static std::unordered_map<std::string, CharAlias> &charMap()
	{
		ensureLoaded();
		return rawCharMap();
	}

	static std::unordered_map<std::string, CharAlias> &rawCharMap()
	{
		static std::unordered_map<std::string, CharAlias> map;
		return map;
	}

	// nattack|attack -> "nattack"; skill1..5 -> "skill1".."skill5"; spc1..3 -> "spc1".."spc3"; lainnya -> ""
	static std::string rangeKey(const std::string &rawKey)
	{
		std::string k = lower(rawKey);
		if (k == "nattack" || k == "attack" || k == "normal")
			return "nattack";
		if (k.size() == 6 && k.compare(0, 5, "skill") == 0 && k[5] >= '1' && k[5] <= '5')
			return k;
		if (k.size() == 4 && k.compare(0, 3, "spc") == 0 && k[3] >= '1' && k[3] <= '3')
			return k;
		return "";
	}

	static std::vector<std::string> &rawMainChars()
	{
		static std::vector<std::string> list;
		return list;
	}

	// Pecah baris seperti: 'Asuma', 'Chiyo', "Choji"  -> Asuma, Chiyo, Choji
	static void parseNameList(const std::string &line, std::vector<std::string> &out)
	{
		std::string token;
		auto flush = [&]()
		{
			if (token.empty())
				return;
			for (auto &n : out)
				if (n == token)
				{
					token.clear();
					return;
				}
			out.push_back(token);
			token.clear();
		};
		for (char c : line)
		{
			if (isalnum((unsigned char)c) || c == '_')
				token.push_back(c);
			else
				flush(); // koma, spasi, kutip, kurung = pemisah
		}
		flush();
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

	// Satu baris section #Char. Bentuk yang didukung:
	//   Madara = Ino
	//   Konohamaru = {Hiruzen, Skill1 = Tobi, skill2 = Kimimaro}
	//   Konohamaru = {            (lanjut di baris berikutnya sampai })
	//       Hiruzen,
	//       skill1 = Tobi,
	//   }
	//   skill1 = Tobi             (menempel ke karakter di atasnya)
	static void parseCharLine(const std::string &line, std::string &currentChar, bool &inBrace)
	{
		bool closes = line.find('}') != std::string::npos;
		if (line.find('{') != std::string::npos)
			inBrace = true;

		std::string text = line;
		for (auto &c : text)
			if (c == '{' || c == '}')
				c = ' ';

		size_t start = 0;
		while (start <= text.size())
		{
			auto comma = text.find(',', start);
			if (comma == std::string::npos)
				comma = text.size();
			std::string seg = trim(text.substr(start, comma - start));
			start = comma + 1;
			if (seg.empty())
				continue;

			auto e = seg.find('=');
			if (e == std::string::npos)
			{
				// nama tanpa '=' di dalam { }: AI untuk karakter di atasnya
				std::string v = unquote(seg);
				if (inBrace && !currentChar.empty() && !v.empty() && rawCharMap()[currentChar].ai.empty())
				{
					rawCharMap()[currentChar].ai = v;
					CCLOG("[AiCompat] Char %s -> AI %s", currentChar.c_str(), v.c_str());
				}
				continue;
			}

			std::string k = unquote(seg.substr(0, e));
			std::string v = unquote(seg.substr(e + 1));
			if (k.empty())
				continue;

			std::string rk = rangeKey(k);
			if (rk.empty())
			{
				// "Nama = Nilai" atau "Nama = {" (AI menyusul di baris berikutnya)
				currentChar = k;
				auto &entry = rawCharMap()[k];
				if (!v.empty())
				{
					entry.ai = v;
					CCLOG("[AiCompat] Char %s -> AI %s", k.c_str(), v.c_str());
				}
				continue;
			}

			if (v.empty())
				continue;
			if (currentChar.empty())
			{
				CCLOG("[AiCompat] %s = %s diabaikan: belum ada karakter di atasnya", k.c_str(), v.c_str());
				continue;
			}
			auto &ranges = rawCharMap()[currentChar].ranges;
			bool replaced = false;
			for (auto &r : ranges)
				if (r.first == rk)
				{
					r.second = v;
					replaced = true;
				}
			if (!replaced)
				ranges.push_back({rk, v});
		}

		if (closes)
			inBrace = false;
	}

	static bool &loaded()
	{
		static bool value = false;
		return value;
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

	static void ensureLoaded()
	{
		if (loaded())
			return;
		loaded() = true;

		unsigned long size = 0;
		unsigned char *data = FileUtils::sharedFileUtils()->getFileData(kFilePath, "r", &size);
		if (!data)
		{
			CCLOG("[AiCompat] %s tidak ditemukan, alias dilewati", kFilePath);
			return;
		}
		std::string text((const char *)data, size);
		delete[] data;

		auto &map = rawMonsterMap();
		std::string section;
		std::string loadText;	 // isi section #Load/UnloadChar (diurai setelah selesai membaca)
		std::string cloneText;	 // isi section #Clone (diurai setelah selesai membaca)
		std::string buffText;	 // isi section #Buff (diurai setelah selesai membaca)
		std::string currentChar; // karakter terakhir di section #Char
		bool inBrace = false;	 // sedang di dalam { ... } yang belum ditutup
		size_t pos = 0;
		while (pos <= text.size())
		{
			auto eol = text.find('\n', pos);
			if (eol == std::string::npos)
				eol = text.size();
			std::string line = trim(text.substr(pos, eol - pos));
			pos = eol + 1;

			// komentar
			if (line.empty() || line.rfind("--", 0) == 0 || line.rfind("//", 0) == 0)
				continue;

			// section: #Monster
			if (line[0] == '#')
			{
				std::string name = lower(trim(line.substr(1)));
				// abaikan baris dekorasi seperti "# =====" / "#  teks bebas"
				bool isSection = !name.empty() && name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_/") == std::string::npos;
				if (isSection)
				{
					section = name;
					continue;
				}
				// Di section #Bullets, baris "#RinSRK = TentenSRK" juga dianggap entri.
				std::string rest = trim(line.substr(1));
				if ((section == "bullet" || section == "bullets") && rest.find('=') != std::string::npos)
					line = rest;
				else
					continue;
			}

			// komentar di akhir baris
			auto cmt = line.find("--");
			if (cmt != std::string::npos)
				line = trim(line.substr(0, cmt));
			if (line.empty())
				continue;

			// section berisi daftar nama: #MainChar
			if (section == "mainchar" || section == "mainchars")
			{
				parseNameList(line, rawMainChars());
				continue;
			}

			// section Ai Summon: #Kugutsu / #Kuchiyose / #Ninja (daftar unit) dan #Clone (definisi)
			if (section == "kugutsu" || section == "puppet")
			{
				SummonConfig::declare(SummonKind::Kugutsu, line);
				continue;
			}
			if (section == "kuchiyose" || section == "summon")
			{
				SummonConfig::declare(SummonKind::Kuchiyose, line);
				continue;
			}
			if (section == "ninja")
			{
				SummonConfig::declare(SummonKind::Ninja, line);
				continue;
			}
			if (section == "catch")
			{
				auto ceq = line.find('=');
				if (ceq != std::string::npos)
				{
					std::string ck = unquote(line.substr(0, ceq));
					std::string cv = unquote(line.substr(ceq + 1));
					if (!ck.empty() && !cv.empty())
					{
						rawCatchMap()[ck] = cv;
						CCLOG("[AiCompat] Catch %s -> ct_hit seperti %s", ck.c_str(), cv.c_str());
					}
				}
				continue;
			}
			if (section == "buff" || section == "buffs")
			{
				buffText += line;
				buffText += '\n';
				continue;
			}
			if (section == "clone" || section == "clones")
			{
				cloneText += line;
				cloneText += '\n';
				continue;
			}

			// section #Char: "Madara = Ino", "Konohamaru = {Hiruzen, skill1 = Tobi}", atau berbaris-baris
			if (isCharSection(section))
			{
				parseCharLine(line, currentChar, inBrace);
				continue;
			}

			// section #Load/UnloadChar: dikumpulkan dulu, diurai di akhir
			if (isLoadSection(section))
			{
				loadText += line;
				loadText += '\n';
				continue;
			}

			auto eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			std::string key = trim(line.substr(0, eq));
			std::string val = trim(line.substr(eq + 1));
			if (key.empty() || val.empty())
				continue;

			if (section == "bullet" || section == "bullets")
			{
				parseAlias(key, val, rawBulletMap(), "Bullet");
				continue;
			}

			if (section == "trap" || section == "traps")
			{
				parseAlias(key, val, rawTrapMap(), "Trap");
				continue;
			}

			if (section == "transform")
			{
				key = unquote(key);
				val = unquote(val);
				if (!key.empty() && !val.empty())
				{
					rawTransformMap()[key] = val;
					CCLOG("[AiCompat] Transform %s -> %s", key.c_str(), val.c_str());
				}
				continue;
			}

			if (section == "monster" || section == "mon" || section == "mons")
			{
				parseAlias(key, val, map, "Monster");
			}
		}

		if (!loadText.empty())
			parseLoadConfig(loadText);

		if (!cloneText.empty())
			SummonConfig::parseClone(cloneText);
		mergeSummonPlists();

		if (!buffText.empty())
			BuffConfig::parse(buffText);
	}

	// Plist unit yang dipanggil ikut dimuat/dilepas bersama pemiliknya (tanpa menulis #Load/UnloadChar).
	static void mergeSummonPlists()
	{
		for (auto &kv : SummonConfig::allDefs())
		{
			auto &entries = rawPlistConfig()[kv.first];
			for (auto &d : kv.second)
			{
				auto *decl = SummonConfig::unit(d.unit);
				if (!decl)
					continue;
				std::vector<std::pair<std::string, std::string>> add;
				if (decl->kind == SummonKind::Kuchiyose)
					add.push_back({"kuchiyose", d.unit});
				else if (decl->kind == SummonKind::Kugutsu)
					add.push_back({"kugutsu", d.unit});
				else if (decl->kind == SummonKind::Ninja)
				{
					add.push_back({"ninja", d.unit});
					add.push_back({"skill", d.unit});
				}
				for (auto &e : add)
				{
					bool dup = false;
					for (auto &x : entries)
						if (x.first == e.first && x.second == e.second)
							dup = true;
					if (!dup)
						entries.push_back(e);
				}
			}
		}
	}
};
