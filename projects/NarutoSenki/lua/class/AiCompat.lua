-- ============================================================
--  AiCompat.lua  -  DATA alias / summon / buff (dibaca C++: Classes/Systems/AiCompat.hpp)
--  Cara menulisnya: lihat lua/class/tutorial.ini
--  Baris '#Nama' = section; baris diawali '--' atau '//' = komentar.
-- ============================================================

#Monster
Chibaku = JibakuEX
Rashomon = Rocket
HiraishinMarkET = HiraishinMark,MinatoET
IndraSusano = ItachiSusano, Indra
Kage2 = Kage, Shikaku, KageHand2
KageHand2 = KageHand

#Bullets
RinSRK = TentenSRK
Shinteshin2 = Shintenshin,InoWar
HiraishinKunaiET = HiraishinKunai,MinatoET

#Characters
Guy = Itachi
Guy8Gates = Chiyo
Hagoromo = Lee
Shikaku = Tobi
InoWar = Naruto
Indra = Tsunade
MinatoET = Kisame
Madara = Ino
Konohamaru = {Hiruzen, Skill1 = Tobi, skill2 = Kimimaro, skill4 = Sakura, Skill5 = Kakashi}
Kabuto = Chiyo
MG = Jiraiya
Zetsu = Kankuro
Danzo = Ino
Ashura = Kisame
DeidaraET = Tsunade
NagatoET = Kisame
Rhino = NarakaPath

#Traps
Amaterasu2 = Amaterasu
KageBom2 = KageBom

#Transform
Guy = Guy8Gates

#Load/UnloadChar
Guy = {
        ninja = 'Guy8Gates',
        skill = 'Guy8Gates',
        skill2 = 'Guy8Gates',
        -- Guy8Gates memakai AI Chiyo, jadi Parents (kugutsu Chiyo) perlu dimuat juga
        Kugutsu = 'Parents',
    },

Zetsu = {
        ninja = 'Zetsu2',
        skill = 'Zetsu2',
        Kuchiyose = 'WhiteZetsu',
        },

Kabuto = {
        Kugutsu = 'SasoriET',
        },

#Kugutsu
SasoriET = Saso

#Kuchiyose
Birds = Centipede
Rhino
Puppet
MotoiOwl
WhiteZetsu

#Ninja
Ashura

#Clone
Ashura = {Ashura, Id = 0, closing = timer, Attack = skill03}       -- clone biasa seperti Naruto
Kabuto = {SasoriET, Id = 3, closing = dead}                        -- panggilan seperti Kankuro (Id 3), tanpa batas waktu
DeidaraET = {Birds, Id = 0, closing = timer}                       -- Deidara memanggil Birds (AI Centipede)
NagatoET = {Rhino, Id = 0, Area = 0, target = shinobi, Attack = skill03+skill02, closing = timer}
Hiruko = {Puppet, Id = 0, Area = 0, setMonAttack = 1 = skill01, untarget = tower, closing = dead, lock = skill01, unlock = skill02 + skill03, Attack = skill02}
Motoi = {MotoiOwl, Id = 0, Area = free, target = shinobi, closing = timer, lock = skill02, Attack = skill01+skill02+skill03}
Zetsu = { WhiteZetsu, Id = 0,Area = 3, Stride, untarget = clone,setMonAttack = 1 = skill01, lock = skill01,unlock = skill02 }

#Catch
Rashomon = SandBall
Ashura = Lee

#Buff
Kabuto = {id = 11, idle = Skill10, Walk = Skill11, nAttack = Skill12, Armored, Imun, closing = timer, lock = skill02+skill03}
MG = {id = 17, Dead = skill08, nAttack = skill12, Walk = skill11, Idle = skill10, skill01 = skill13, skill02 = skill14, skill03 = skill15, Hide = skill04+skill05, Armored, closing = setActionResume + timer > skill09}
Zetsu = {id = 17, idle = Skill10, Walk = Skill11, nAttack = Skill12, closing = clone}
Danzo = {id = 17, nAttack = skill10, closing = timer, Skills = Lock}

#MainChar
'Asuma', 'Chiyo', 'Choji', 'Deidara', 'Gaara', 'Hidan', 'Hinata', 'Hiruzen', 'Itachi', 'Jiraiya', 'Jugo', 'Kakashi', 'Kakuzu', 'Kankuro', 'Karin', 'Kiba', 'Kimimaro', 'Kisame', 'Konan', 'Lee', 'Minato', 'Naruto', 'Neji', 'Ino', 'Orochimaru', 'Pain', 'Sai', 'Sakura', 'Sasuke', 'Shikamaru', 'Shino', 'Suigetsu', 'Tenten', 'Tobi', 'Tobirama', 'Tsunade',
