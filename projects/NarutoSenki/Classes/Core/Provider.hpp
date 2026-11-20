#pragma once

#include "Systems/AiCompat.hpp"

// Shinobi
#include "Shinobi/Asuma.hpp"
#include "Shinobi/Chiyo.hpp"
#include "Shinobi/Choji.hpp"
#include "Shinobi/Deidara.hpp"
#include "Shinobi/Gaara.hpp"
#include "Shinobi/Hidan.hpp"
#include "Shinobi/Hinata.hpp"
#include "Shinobi/Hiruzen.hpp"
#include "Shinobi/Ino.hpp"
#include "Shinobi/Itachi.hpp"
#include "Shinobi/Jiraiya.hpp"
#include "Shinobi/Jugo.hpp"
#include "Shinobi/Kakashi.hpp"
#include "Shinobi/Kakuzu.hpp"
#include "Shinobi/Kankuro.hpp"
#include "Shinobi/Karin.hpp"
#include "Shinobi/Kiba.hpp"
#include "Shinobi/Kimimaro.hpp"
#include "Shinobi/Kisame.hpp"
#include "Shinobi/Konan.hpp"
#include "Shinobi/Lee.hpp"
#include "Shinobi/Minato.hpp"
// #include "Shinobi/Nagato.hpp"
#include "Shinobi/Naruto.hpp"
#include "Shinobi/Neji.hpp"
#include "Shinobi/Orochimaru.hpp"
#include "Shinobi/Pain.hpp"
#include "Shinobi/Sai.hpp"
#include "Shinobi/Sakura.hpp"
#include "Shinobi/Sasuke.hpp"
#include "Shinobi/Shikamaru.hpp"
#include "Shinobi/Shino.hpp"
#include "Shinobi/Suigetsu.hpp"
#include "Shinobi/Tenten.hpp"
#include "Shinobi/Tobi.hpp"
#include "Shinobi/Tobirama.hpp"
#include "Shinobi/Tsunade.hpp"

// Bunshin
#include "Shinobi/Bunshin/NarutoClone.hpp"
#include "Shinobi/Bunshin/SageNarutoClone.hpp"
#include "Shinobi/Bunshin/RikudoNarutoClone.hpp"

// Guardian
// Han, Roshi
#include "Guardian/Guardian.hpp"

// Jutsu

// Kuchiyose
#include "Kuchiyose/Akamaru.hpp"
#include "Kuchiyose/Centipede.hpp"
#include "Kuchiyose/DogWall.hpp"
#include "Kuchiyose/Karasu.hpp"
#include "Kuchiyose/LuaSummon.hpp"
#include "Kuchiyose/Kurama.hpp"
#include "Kuchiyose/Mask.hpp"
#include "Kuchiyose/Parents.hpp"
#include "Kuchiyose/Sanshouuo.hpp"
#include "Kuchiyose/Saso.hpp"
#include "Kuchiyose/Slug.hpp"

#define __begin__ if(0) {}
#define is(var) else if (aiName == var)
#define is_or(var1, var2) else if (aiName == var1 || aiName == var2)
#define is_role(_Role) if (role == _Role)

// Didefinisikan di CharacterBase.cpp (SummonSystem::applyBaseline).
void SummonSystemApplyBaseline(CharacterBase *owner);

// TODO: Impl a default AI logic
class DefaultAI : public Hero
{
	void perform() override
	{
	}
};

class Provider
{
private:
	// Salin jarak serangan (attackRangeX/Y) dari karakter donor ke skill milik `hero`.
	// Daftar donor diatur di AiCompat.lua, mis. "Konohamaru = Hiruzen, skill1 = Tobi".
	static void applyAttackRanges(Hero *hero, const string &name)
	{
		for (const auto &entry : AiCompat::charRanges(name))
		{
			const string &key = entry.first;
			const string &donor = entry.second;

			// indeks aksi di xml: 7 = nattack, 8..12 = skill1..5, 17..19 = spc1..3
			int index = -1;
			if (key == "nattack")
				index = 7;
			else if (key.size() == 6 && key.compare(0, 5, "skill") == 0)
				index = 7 + (key[5] - '0');
			else if (key.size() == 4 && key.compare(0, 3, "spc") == 0)
				index = 16 + (key[3] - '0');
			if (index < 0)
				continue;

			static const char *kDirs[] = {"Ninja", "Kuchiyose", "Kugutsu", "Guardian"};
			auto fileUtils = FileUtils::sharedFileUtils();
			string fpath;
			for (auto dir : kDirs)
			{
				string path = fileUtils->fullPathForFilename(format("Unit/{}/{}/{}.xml", dir, donor, donor).c_str());
				if (fileUtils->isFileExist(path))
				{
					fpath = path;
					break;
				}
			}
			if (fpath.empty())
			{
				CCLOG("[Provider] %s.%s dilewati: data donor %s tidak ada", name.c_str(), key.c_str(), donor.c_str());
				continue;
			}

			CCArray *animationArray = CCArray::create();
			if (!KTools::readXMLToArray(fpath, animationArray) || (int)animationArray->count() <= index)
			{
				CCLOG("[Provider] %s.%s dilewati: %s tidak punya aksi ke-%d", name.c_str(), key.c_str(), donor.c_str(), index);
				continue;
			}

			CCArray *tmpAction = (CCArray *)(animationArray->objectAtIndex(index));
			CCArray *tmpData = (CCArray *)(tmpAction->objectAtIndex(0));

			string type;
			uint32_t value = 0, cd = 0;
			int rangeX = 0, rangeY = 0, combatPoint = 0;
			hero->readData(tmpData, type, value, rangeX, rangeY, cd, combatPoint);

			if (key == "nattack")
			{
				hero->_nAttackRangeX = rangeX;
				hero->_nAttackRangeY = rangeY;
			}
			else if (key == "skill1")
			{
				hero->_sAttackRangeX1 = rangeX;
				hero->_sAttackRangeY1 = rangeY;
			}
			else if (key == "skill2")
			{
				hero->_sAttackRangeX2 = rangeX;
				hero->_sAttackRangeY2 = rangeY;
			}
			else if (key == "skill3")
			{
				hero->_sAttackRangeX3 = rangeX;
				hero->_sAttackRangeY3 = rangeY;
			}
			else if (key == "skill4")
			{
				hero->_sAttackRangeX4 = rangeX;
				hero->_sAttackRangeY4 = rangeY;
			}
			else if (key == "skill5")
			{
				hero->_sAttackRangeX5 = rangeX;
				hero->_sAttackRangeY5 = rangeY;
			}
			else if (key == "spc1")
			{
				hero->_spcAttackRangeX1 = rangeX;
				hero->_spcAttackRangeY1 = rangeY;
			}
			else if (key == "spc2")
			{
				hero->_spcAttackRangeX2 = rangeX;
				hero->_spcAttackRangeY2 = rangeY;
			}
			else if (key == "spc3")
			{
				hero->_spcAttackRangeX3 = rangeX;
				hero->_spcAttackRangeY3 = rangeY;
			}
			CCLOG("[Provider] %s.%s jarak serangan dari %s: %d, %d", name.c_str(), key.c_str(), donor.c_str(), rangeX, rangeY);
		}
	}

public:
	static Hero *create(const string &name, Role role, Group group)
	{
		Hero *ptr;

		// AiCompat.lua (#Char): "Madara = Ino" -> Madara memakai kelas AI Ino.
		// aiName memilih kelas AI di bawah, resName adalah nama data (xml/plist) yang dimuat.
		// Unit yang dideklarasikan di #Kugutsu/#Kuchiyose/#Ninja: AI dari "Nama = AI" atau #Characters.
		const bool isLuaSummonUnit = AiCompat::summonKind(name) != SummonKind::None;
		const string aiName = isLuaSummonUnit ? AiCompat::resolveSummonAI(name) : AiCompat::resolveChar(name);
		string resName = name;
		if (aiName != name && !AiCompat::unitXmlExists(name))
		{
			// Tidak punya data sendiri -> pakai data milik AI-nya.
			resName = aiName;
		}

		__begin__
		is("Akamaru") 						ptr = new Akamaru();
		is("Asuma") 						ptr = new Asuma();
		is("Centipede") 					ptr = new Centipede();
		is("Chiyo") 						ptr = new Chiyo();
		is("Choji") 						ptr = new Choji();
		is("Deidara") 						ptr = new Deidara();
		is("DogWall") 						ptr = new DogWall();
		is("Gaara") 						ptr = new Gaara();
		is("Hidan") 						ptr = new Hidan();
		is("Hinata") 						ptr = new Hinata();
		is("Hiruzen") 						ptr = new Hiruzen();
		is("Ino") 							ptr = new Ino();
		is("Itachi") 						ptr = new Itachi();
		is_or("Jiraiya", "SageJiraiya") 	ptr = new Jiraiya();
		is("Jugo") 							ptr = new Jugo();
		is("Kakashi") 						ptr = new Kakashi();
		is("Kakuzu") 						ptr = new Kakuzu();
		is("Kankuro") 						ptr = new Kankuro();
		is("Karasu") 						ptr = new Karasu();
		is("Karin") 						ptr = new Karin();
		is("Kiba") 							ptr = new Kiba();
		is("Kimimaro") 						ptr = new Kimimaro();
		is("Kisame") 						ptr = new Kisame();
		is("Konan") 						ptr = new Konan();
		is("Kurama") 						ptr = new Kurama();
		is_or("Lee", "RockLee") 			ptr = new Lee();
		is("MaskFuton") 					ptr = new Mask();
		is("MaskKaton") 					ptr = new Mask();
		is("MaskRaiton") 					ptr = new Mask();
		is("Minato") 						ptr = new Minato();
		is("Naruto") 						is_role(Role::Clone) ptr = new NarutoClone();		else ptr = new Naruto();
		is("RikudoNaruto") 					is_role(Role::Clone) ptr = new RikudoNarutoClone();	else ptr = new Naruto();
		is("SageNaruto") 					is_role(Role::Clone) ptr = new SageNarutoClone();	else ptr = new Naruto();
		is("Neji") 							ptr = new Neji();
		is("Orochimaru") 					ptr = new Orochimaru();
		is_or("Pain", "Nagato") 			ptr = new Pain();
		is("AnimalPath") 					ptr = new AnimalPath();
		is("AsuraPath") 					ptr = new AsuraPath();
		is("NarakaPath") 					ptr = new NarakaPath();
		is("PertaPath") 					ptr = new PertaPath();
		is("HumanPath") 					ptr = new HumanPath();
		is("Sai") 							ptr = new Sai();
		is("Sakura") 						ptr = new Sakura();
		is("Sanshouuo") 					ptr = new Sanshouuo();
		is("Saso") 							ptr = new Saso();
		is_or("Sasuke", "ImmortalSasuke") 	ptr = new Sasuke();
		is("Shikamaru") 					ptr = new Shikamaru();
		is("Shino") 						ptr = new Shino();
		is("Slug") 							ptr = new Slug();
		is("Suigetsu") 						ptr = new Suigetsu();
		is("Tenten") 						ptr = new Tenten();
		is("Tobi") 							ptr = new Tobi();
		is("Tobirama") 						ptr = new Tobirama();
		is("Tsunade") 						ptr = new Tsunade();
		is_or("Roshi", "Han")				ptr = new Guardian();
		else if (isLuaSummonUnit)			ptr = new LuaSummon();
		else								ptr = new DefaultAI();

		if (!ptr)
		{
			CCLOG("Not found character [ %s ]", name.c_str());
			return nullptr;
		}

		if (ptr->init())
		{
			ptr->setID(resName, role, group);
			applyAttackRanges(ptr, name);
			ptr->autorelease();
			// Kunci awal skill dari "unlock" di #Clone (hanya berarti untuk pemilik summon).
			SummonSystemApplyBaseline(ptr);
		}
		else
		{
			CCLOG("Set character %s not found", name.c_str());
			delete ptr;
			ptr = nullptr;
		}
		return ptr;
	}
};

#undef __begin__
#undef is
#undef is_or
#undef is_role
