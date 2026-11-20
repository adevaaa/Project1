#pragma once
#include "Data/UnitDefines.h"
#include "Systems/AiCompat.hpp"

#include <string>
#include <vector>

/**
 * HeroRoster
 *
 * Daftar karakter utama yang boleh masuk battle (dipakai untuk COM, musuh acak,
 * getRandomHero*, dan seed CharRecord). Menggantikan pemakaian langsung kHeroList.
 *
 * Sumber:
 *   1. Section #MainChar di lua/class/AiCompat.lua
 *   2. Jika section kosong / tidak ada / semua nama tidak valid -> kHeroList (UnitDefines.h)
 *
 * Sebuah nama hanya masuk jika Resources/Unit/Ninja/<Nama>/<Nama>.xml ada,
 * karena tanpa data itu Hero::init gagal saat battle.
 *
 * Daftar dibangun sekali dan tidak berubah lagi, jadi pointer dari at() aman
 * dipakai seperti isi kHeroList sebelumnya.
 */
class HeroRoster
{
public:
	static size_t size() { return list().size(); }

	static const char *at(size_t i) { return list()[i].c_str(); }

	static const std::vector<std::string> &all() { return list(); }

private:
	static const std::vector<std::string> &list()
	{
		static const std::vector<std::string> roster = build();
		return roster;
	}

	static std::vector<std::string> build()
	{
		std::vector<std::string> result;
		for (const auto &name : AiCompat::mainChars())
		{
			if (AiCompat::heroXmlExists(name))
				result.push_back(name);
			else
				CCLOG("[HeroRoster] %s dilewati: Unit/Ninja/%s/%s.xml tidak ada", name.c_str(), name.c_str(), name.c_str());
		}

		if (result.empty())
		{
			for (size_t i = 0; i < kHeroNum; i++)
				result.push_back(kHeroList[i]);
		}
		else
		{
			CCLOG("[HeroRoster] %d karakter dimuat dari AiCompat.lua", (int)result.size());
		}
		return result;
	}
};
