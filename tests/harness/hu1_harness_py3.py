# -*- coding: utf-8 -*-

"""
Liblouis test harness for the Hungarian grade 1 table.
Please see the liblouis documentationfor more information.
"""

import sys
import louis

table = 'hu1.ctb'

tests = [
    {
        'txt':  'adásszerű', 
        'brl':  'ad"s5er}', 
    }, {
        'txt':  'adásszünet', 
        'brl':  'ad"s5{net', 
    }, {
        'txt':  'adósszámla', 
        'brl':  'ad9s5"mla', 
    }, {
        'txt':  'Agyagosszergény', 
        'brl':  '$a4agos5erg16', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'agyaggyűrűiken', 
        'brl':  'a4ag4}r}iken', 
    }, {
        'txt':  'agyonnyom', 
        'brl':  'a4on6om', 
    }, {
        'txt':  'agyonnyúzott', 
        'brl':  'a4on602ott', 
    }, {
        'txt':  'agyonnyűtt ', 
        'brl':  'a4on6}tt ', 
    }, {
        'txt':  'ágyússzekerek', 
        'brl':  '"40s5ekerek', 
    }, {
        'txt':  'ájulásszerű', 
        'brl':  '"jul"s5er}', 
    }, {
        'txt':  'akácsövény', 
        'brl':  'ak"csqv16', 
    }, {
        'txt':  'ákácsövény', 
        'brl':  '"k"csqv16', 
    }, {
        'txt':  'alásszolgája', 
        'brl':  'al"s5olg"ja', 
    }, {
        'txt':  'alásszolgájuk ', 
        'brl':  'al"s5olg"juk ', 
    }, {
        'txt':  'alkalmazásszerver', 
        'brl':  'alkalma2"s5erver', 
    }, {
        'txt':  'állásszög', 
        'brl':  '"ll"s5qg', 
    }, {
        'txt':  'almásszürke', 
        'brl':  'alm"s5{rke', 
    }, {
        'txt':  'alvásszegény', 
        'brl':  'alv"s5eg16', 
    }, {
        'txt':  'alvásszükséglete', 
        'brl':  'alv"s5{ks1glete', 
    }, {
        'txt':  'alvászavar', 
        'brl':  'alv"s2avar', 
    }, {
        'txt':  'anyaggyőző', 
        'brl':  'a6ag4727', 
    }, {
        'txt':  'anyaggyűjtés', 
        'brl':  'a6ag4}jt1s', 
    }, {
        'txt':  'aranyosszőke', 
        'brl':  'ara6os57ke', 
    }, {
        'txt':  'árboccsúcs', 
        'brl':  '"rboc303', 
    }, {
        'txt':  'árbóccsúcs', 
        'brl':  '"rb9c303', 
    }, {
        'txt':  'árbocsudarat', 
        'brl':  '"rbocsudarat', 
    }, {
        'txt':  'arcüreggyulladás', 
        'brl':  'arc{reg4ullad"s', 
    }, {
        'txt':  'arccsont', 
        'brl':  'arc3ont', 
    }, {
        'txt':  'arcseb', 
        'brl':  'arcseb', 
    }, {
        'txt':  'arcsebe ', 
        'brl':  'arcsebe ', 
    }, {
        'txt':  'arcsérülés', 
        'brl':  'arcs1r{l1s', 
    }, {
        'txt':  'árgusszemű', 
        'brl':  '"rgus5em}', 
    }, {
        'txt':  'árvízsújtotta', 
        'brl':  '"rv|2s0jtotta', 
    }, {
        'txt':  'autósszemüveg', 
        'brl':  'aut9s5em{veg', 
    }, {
        'txt':  'azonnyomban', 
        'brl':  'a2on6omban', 
    }, {
        'txt':  'bádoggyűjtemény', 
        'brl':  'b"dog4}jtem16', 
    }, {
        'txt':  'barnásszőke', 
        'brl':  'barn"s57ke', 
    }, {
        'txt':  'barnásszőkét', 
        'brl':  'barn"s57k1t', 
    }, {
        'txt':  'barnásszürke', 
        'brl':  'barn"s5{rke', 
    }, {
        'txt':  'barnesszal', 
        'brl':  'barne55al', 
    }, {
        'txt':  'becslésszerűen', 
        'brl':  'be3l1s5er}en', 
    }, {
        'txt':  'Békésszentandrás', 
        'brl':  '$b1k1s5entandr"s', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'bélésszövet', 
        'brl':  'b1l1s5qvet', 
    }, {
        'txt':  'bélyeggyűjtemény', 
        'brl':  'b1eg4}jtem16', 
    }, {
        'txt':  'bércsüveg', 
        'brl':  'b1rcs{veg', 
    }, {
        'txt':  'berendezésszerű ', 
        'brl':  'berende21s5er} ', 
    }, {
        'txt':  'berendezésszett', 
        'brl':  'berende21s5ett', 
    }, {
        'txt':  'beteggyógyász', 
        'brl':  'beteg494"5', 
    }, {
        'txt':  'bilincszörgés', 
        'brl':  'bilin32qrg1s', 
    }, {
        'txt':  'binsenggyökér', 
        'brl':  'binsen44qk1r', 
    }, {
        'txt':  'bohócsapkája', 
        'brl':  'boh9csapk"ja', 
    }, {
        'txt':  'bonbonmeggy', 
        'brl':  'bonbonme44', 
    }, {
        'txt':  'borsszem', 
        'brl':  'bors5em', 
    }, {
        'txt':  'borsszóró', 
        'brl':  'bors59r9', 
    }, {
        'txt':  'borzasszőrű', 
        'brl':  'bor2as57r}', 
    }, {
        'txt':  'borzzsír', 
        'brl':  'bor2`|r', 
    }, {
        'txt':  'bőgésszerű', 
        'brl':  'b7g1s5er}', 
    }, {
        'txt':  'börtönnyelve', 
        'brl':  'bqrtqn6elve', 
    }, {
        'txt':  'brekegésszerű', 
        'brl':  'brekeg1s5er}', 
    }, {
        'txt':  'bronzsáska', 
        'brl':  'bron2s"ska', 
    }, {
        'txt':  'bronzsáskák', 
        'brl':  'bron2s"sk"k', 
    }, {
        'txt':  'bronzsasokkal ', 
        'brl':  'bron2sasokkal ', 
    }, {
        'txt':  'bronzsisak', 
        'brl':  'bron2sisak', 
    }, {
        'txt':  'búcsújárásszerű', 
        'brl':  'b030j"r"s5er}', 
    }, {
        'txt':  'bűnnyomok', 
        'brl':  'b}n6omok', 
    }, {
        'txt':  'chipseszacskó', 
        'brl':  'chipses2a3k9', 
    }, {
        'txt':  'csapásszám', 
        'brl':  '3ap"s5"m', 
    }, {
        'txt':  'csárdásszóló', 
        'brl':  '3"rd"s59l9', 
    }, {
        'txt':  'csattanásszerű', 
        'brl':  '3attan"s5er}', 
    }, {
        'txt':  'csavarásszerű', 
        'brl':  '3avar"s5er}', 
    }, {
        'txt':  'csikósszámadó', 
        'brl':  '3ik9s5"mad9', 
    }, {
        'txt':  'csipkésszélű', 
        'brl':  '3ipk1s51l}', 
    }, {
        'txt':  'csobbanásszerű', 
        'brl':  '3obban"s5er}', 
    }, {
        'txt':  'csuklásszerű', 
        'brl':  '3ukl"s5er}', 
    }, {
        'txt':  'disszertáció', 
        'brl':  'di55ert"ci9', 
    }, {
        'txt':  'dobpergésszerűen', 
        'brl':  'dobperg1s5er}en', 
    }, {
        'txt':  'döggyapjú', 
        'brl':  'dqg4apj0', 
    }, {
        'txt':  'dőlésszög', 
        'brl':  'd7l1s5qg', 
    }, {
        'txt':  'dörgésszerű', 
        'brl':  'dqrg1s5er}', 
    }, {
        'txt':  'dörgésszerű ', 
        'brl':  'dqrg1s5er} ', 
    }, {
        'txt':  'dragonyosszázad ', 
        'brl':  'drago6os5"2ad ', 
    }, {
        'txt':  'dragonyoszászlóalj', 
        'brl':  'drago6os2"5l9alj', 
    }, {
        'txt':  'droggyanús', 
        'brl':  'drog4an0s', 
    }, {
        'txt':  'dússzakállú', 
        'brl':  'd0s5ak"ll0', 
    }, {
        'txt':  'édesszájú', 
        'brl':  '1des5"j0', 
    }, {
        'txt':  'édesszesztestvér', 
        'brl':  '1des5e5testv1r', 
    }, {
        'txt':  'égésszabály', 
        'brl':  '1g1s5ab"', 
    }, {
        'txt':  'égésszag', 
        'brl':  '1g1s5ag', 
    }, {
        'txt':  'égésszám', 
        'brl':  '1g1s5"m', 
    }, {
        'txt':  'égésszigetelés', 
        'brl':  '1g1s5igetel1s', 
    }, {
        'txt':  'egyenesszálú', 
        'brl':  'e4enes5"l0', 
    }, {
        'txt':  'egyenesszárnyúak', 
        'brl':  'e4enes5"r60ak', 
    }, {
        'txt':  'egyenesszög', 
        'brl':  'e4enes5qg', 
    }, {
        'txt':  'egyezség', 
        'brl':  'e4e2s1g', 
    }, {
        'txt':  'éhesszájat ', 
        'brl':  '1hes5"jat ', 
    }, {
        'txt':  'ejtőernyősszárnyak', 
        'brl':  'ejt7er67s5"r6ak', 
    }, {
        'txt':  'ejtőernyősszázad', 
        'brl':  'ejt7er67s5"2ad', 
    }, {
        'txt':  'ejtőernyőszászlóalj ', 
        'brl':  'ejt7er67s2"5l9alj ', 
    }, {
        'txt':  'ékesszólás', 
        'brl':  '1kes59l"s', 
    }, {
        'txt':  'ékesszóló ', 
        'brl':  '1kes59l9 ', 
    }, {
        'txt':  'ekhósszekér', 
        'brl':  'ekh9s5ek1r', 
    }, {
        'txt':  'ekhósszekerek', 
        'brl':  'ekh9s5ekerek', 
    }, {
        'txt':  'eleséggyűjtés ', 
        'brl':  'eles1g4}jt1s ', 
    }, {
        'txt':  'élesszemű', 
        'brl':  '1les5em}', 
    }, {
        'txt':  'ellátásszerű', 
        'brl':  'ell"t"s5er}', 
    }, {
        'txt':  'ellenállásszekrény', 
        'brl':  'ellen"ll"s5ekr16', 
    }, {
        'txt':  'ellennyilatkozat', 
        'brl':  'ellen6ilatko2at', 
    }, {
        'txt':  'ellennyomás', 
        'brl':  'ellen6om"s', 
    }, {
        'txt':  'elméncség', 
        'brl':  'elm1ncs1g', 
    }, {
        'txt':  'előírásszerű ', 
        'brl':  'el7|r"s5er} ', 
    }, {
        'txt':  'elrémisszék ', 
        'brl':  'elr1mi551k ', 
    }, {
        'txt':  'emberhússzagot ', 
        'brl':  'emberh0s5agot ', 
    }, {
        'txt':  'emelésszerű', 
        'brl':  'emel1s5er}', 
    }, {
        'txt':  'érccsapadék, érccsengés, érccsatorna ', 
        'brl':  '1rc3apad1k, 1rc3eng1s, 1rc3atorna ', 
    }, {
        'txt':  'ércsalak', 
        'brl':  '1rcsalak', 
    }, {
        'txt':  'ércsas', 
        'brl':  '1rcsas', 
    }, {
        'txt':  'ércselyem', 
        'brl':  '1rcseem', 
    }, {
        'txt':  'ércsíp, ércsípjába, ércsípláda ', 
        'brl':  '1rcs|p, 1rcs|pj"ba, 1rcs|pl"da ', 
    }, {
        'txt':  'ércsíptető', 
        'brl':  '1r3|ptet7', 
    }, {
        'txt':  'ércsisak', 
        'brl':  '1rcsisak', 
    }, {
        'txt':  'ércsodrony', 
        'brl':  '1rcsodro6', 
    }, {
        'txt':  'erőforrászabáló', 
        'brl':  'er7forr"s2ab"l9', 
    }, {
        'txt':  'érzékelésszint', 
        'brl':  '1r21kel1s5int', 
    }, {
        'txt':  'ésszerű', 
        'brl':  '155er}', 
    }, {
        'txt':  'eszközsor, eszközsorán', 
        'brl':  'e5kq2sor, e5kq2sor"n', 
    }, {
        'txt':  'evészavar', 
        'brl':  'ev1s2avar', 
    }, {
        'txt':  'fagyosszentek', 
        'brl':  'fa4os5entek', 
    }, {
        'txt':  'fáklyászene', 
        'brl':  'f"k"s2ene', 
    }, {
        'txt':  'farkasszáj ', 
        'brl':  'farkas5"j ', 
    }, {
        'txt':  'farkasszem', 
        'brl':  'farkas5em', 
    }, {
        'txt':  'farkasszemet ', 
        'brl':  'farkas5emet ', 
    }, {
        'txt':  'Farkassziget,', 
        'brl':  '$farkas5iget,', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'fásszárú', 
        'brl':  'f"s5"r0', 
    }, {
        'txt':  'fegyenccsoport', 
        'brl':  'fe4enc3oport', 
    }, {
        'txt':  'fegyencsapkát ', 
        'brl':  'fe4encsapk"t ', 
    }, {
        'txt':  'fehéresszőke, fehéresszürke ', 
        'brl':  'feh1res57ke, feh1res5{rke ', 
    }, {
        'txt':  'feketésszürke', 
        'brl':  'feket1s5{rke', 
    }, {
        'txt':  'feleséggyilkos', 
        'brl':  'feles1g4ilkos', 
    }, {
        'txt':  'felfedezésszámba', 
        'brl':  'felfede21s5"mba', 
    }, {
        'txt':  'felséggyilkolás', 
        'brl':  'fels1g4ilkol"s', 
    }, {
        'txt':  'felszerelésszettet', 
        'brl':  'fel5erel1s5ettet', 
    }, {
        'txt':  'fertőzésszerű', 
        'brl':  'fert721s5er}', 
    }, {
        'txt':  'filccsizma', 
        'brl':  'filc3i2ma', 
    }, {
        'txt':  'filigránnyelű', 
        'brl':  'filigr"n6el}', 
    }, {
        'txt':  'fogasszeg', 
        'brl':  'fogas5eg', 
    }, {
        'txt':  'fogfájásszerű', 
        'brl':  'fogf"j"s5er}', 
    }, {
        'txt':  'foglalkozásszerű', 
        'brl':  'foglalko2"s5er}', 
    }, {
        'txt':  'foggyalu', 
        'brl':  'fog4alu', 
    }, {
        'txt':  'foggyökér', 
        'brl':  'fog4qk1r', 
    }, {
        'txt':  'foggyulladás', 
        'brl':  'fog4ullad"s', 
    }, {
        'txt':  'foggyűrű', 
        'brl':  'fog4}r}', 
    }, {
        'txt':  'forgásszabály', 
        'brl':  'forg"s5ab"', 
    }, {
        'txt':  'forrásszöveg', 
        'brl':  'forr"s5qveg', 
    }, {
        'txt':  'fosszínű', 
        'brl':  'fos5|n}', 
    }, {
        'txt':  'földcsuszamlásszerűen', 
        'brl':  'fqld3u5aml"s5er}en', 
    }, {
        'txt':  'fölélesszem', 
        'brl':  'fql1le55em', 
    }, {
        'txt':  'főzőkalánnyelet', 
        'brl':  'f727kal"n6elet', 
    }, {
        'txt':  'fuvarosszekér', 
        'brl':  'fuvaros5ek1r', 
    }, {
        'txt':  'fúvósszerszám', 
        'brl':  'f0v9s5er5"m', 
    }, {
        'txt':  'fúvósszimfónia', 
        'brl':  'f0v9s5imf9nia', 
    }, {
        'txt':  'fúvószenekar', 
        'brl':  'f0v9s2enekar', 
    }, {
        'txt':  'fűtésszag', 
        'brl':  'f}t1s5ag', 
    }, {
        'txt':  'garaboncsereg', 
        'brl':  'garaboncsereg', 
    }, {
        'txt':  'gázspray', 
        'brl':  'g"2spray', 
    }, {
        'txt':  'gázsugár', 
        'brl':  'g"2sug"r', 
    }, {
        'txt':  'gerincsérült', 
        'brl':  'gerincs1r{lt', 
    }, {
        'txt':  'gerincsérv ', 
        'brl':  'gerincs1rv ', 
    }, {
        'txt':  'ginszenggyökér', 
        'brl':  'gin5eng4qk1r', 
    }, {
        'txt':  'ginzenggyökér', 
        'brl':  'gin2eng4qk1r', 
    }, {
        'txt':  'Gombosszeg', 
        'brl':  '$gombos5eg', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'gondviselésszerű', 
        'brl':  'gondvisel1s5er}', 
    }, {
        'txt':  'gőzsíp', 
        'brl':  'g72s|p', 
    }, {
        'txt':  'gőzsugár', 
        'brl':  'g72sug"r', 
    }, {
        'txt':  'gőzszivattyú', 
        'brl':  'g725iva880', 
    }, {
        'txt':  'gránátoszászlóalj', 
        'brl':  'gr"n"tos2"5l9alj', 
    }, {
        'txt':  'gúnyversszerző', 
        'brl':  'g06vers5er27', 
    }, {
        'txt':  'gyalogosszázad', 
        'brl':  '4alogos5"2ad', 
    }, {
        'txt':  'gyalogoszászlóalj,', 
        'brl':  '4alogos2"5l9alj,', 
    }, {
        'txt':  'gyorsszárnyú', 
        'brl':  '4ors5"r60', 
    }, {
        'txt':  'gyorsszekér ', 
        'brl':  '4ors5ek1r ', 
    }, {
        'txt':  'gyorsszűrő', 
        'brl':  '4ors5}r7', 
    }, {
        'txt':  'gyújtásszabály', 
        'brl':  '40jt"s5ab"', 
    }, {
        'txt':  'gyújtászsinór', 
        'brl':  '40jt"s`in9r', 
    }, {
        'txt':  'gyűlésszíne', 
        'brl':  '4}l1s5|ne', 
    }, {
        'txt':  'habarccsal', 
        'brl':  'habar33al', 
    }, {
        'txt':  'habitusszerűen', 
        'brl':  'habitus5er}en', 
    }, {
        'txt':  'hadianyaggyár', 
        'brl':  'hadia6ag4"r', 
    }, {
        'txt':  'hadsereggyűjtés ', 
        'brl':  'hadsereg4}jt1s ', 
    }, {
        'txt':  'hajlásszög', 
        'brl':  'hajl"s5qg', 
    }, {
        'txt':  'hajósszekerce ', 
        'brl':  'haj9s5ekerce ', 
    }, {
        'txt':  'hajósszemélyzet', 
        'brl':  'haj9s5em12et', 
    }, {
        'txt':  'hallászavar', 
        'brl':  'hall"s2avar', 
    }, {
        'txt':  'halottasszekér ', 
        'brl':  'halottas5ek1r ', 
    }, {
        'txt':  'halottasszoba', 
        'brl':  'halottas5oba', 
    }, {
        'txt':  'halottasszobába', 
        'brl':  'halottas5ob"ba', 
    }, {
        'txt':  'hamvasszőke', 
        'brl':  'hamvas57ke', 
    }, {
        'txt':  'hamvasszürke', 
        'brl':  'hamvas5{rke', 
    }, {
        'txt':  'hanggyakorlat', 
        'brl':  'hang4akorlat', 
    }, {
        'txt':  'hányásszag', 
        'brl':  'h"6"s5ag', 
    }, {
        'txt':  'haragoszöld', 
        'brl':  'haragos2qld', 
    }, {
        'txt':  'harcosszellem', 
        'brl':  'harcos5ellem', 
    }, {
        'txt':  'harccselekmény ', 
        'brl':  'harc3elekm16 ', 
    }, {
        'txt':  'harccsoport', 
        'brl':  'harc3oport', 
    }, {
        'txt':  'harcsor', 
        'brl':  'harcsor', 
    }, {
        'txt':  'hármasszámú', 
        'brl':  'h"rmas5"m0', 
    }, {
        'txt':  'hársszén', 
        'brl':  'h"rs51n', 
    }, {
        'txt':  'hársszenet', 
        'brl':  'h"rs5enet', 
    }, {
        'txt':  'hártyásszárnyú', 
        'brl':  'h"r8"s5"r60', 
    }, {
        'txt':  'hasisszagot', 
        'brl':  'hasis5agot', 
    }, {
        'txt':  'hatásszünet', 
        'brl':  'hat"s5{net', 
    }, {
        'txt':  'házsárkodását', 
        'brl':  'h"`"rkod"s"t', 
    }, {
        'txt':  'hegyesszög', 
        'brl':  'he4es5qg', 
    }, {
        'txt':  'hegyszorosszerű', 
        'brl':  'he45oros5er}', 
    }, {
        'txt':  'hekusszagot', 
        'brl':  'hekus5agot', 
    }, {
        'txt':  'hentesszaktanfolyamát', 
        'brl':  'hentes5aktanfoam"t', 
    }, {
        'txt':  'hirdetésszöveg', 
        'brl':  'hirdet1s5qveg', 
    }, {
        'txt':  'hivatásszerűen', 
        'brl':  'hivat"s5er}en', 
    }, {
        'txt':  'hízelkedésszámba', 
        'brl':  'h|2elked1s5"mba', 
    }, {
        'txt':  'hólyaggyulladás', 
        'brl':  'h9ag4ullad"s', 
    }, {
        'txt':  'hörgésszerű', 
        'brl':  'hqrg1s5er}', 
    }, {
        'txt':  'hősszínész', 
        'brl':  'h7s5|n15', 
    }, {
        'txt':  'hősszövetség ', 
        'brl':  'h7s5qvets1g ', 
    }, {
        'txt':  'hússzaft', 
        'brl':  'h0s5aft', 
    }, {
        'txt':  'hússzag', 
        'brl':  'h0s5ag', 
    }, {
        'txt':  'hússzagú', 
        'brl':  'h0s5ag0', 
    }, {
        'txt':  'hússzállítmány ', 
        'brl':  'h0s5"ll|tm"6 ', 
    }, {
        'txt':  'hússzállító', 
        'brl':  'h0s5"ll|t9', 
    }, {
        'txt':  'hússzalonna', 
        'brl':  'h0s5alonna', 
    }, {
        'txt':  'hússzekrény', 
        'brl':  'h0s5ekr16', 
    }, {
        'txt':  'hússzelet', 
        'brl':  'h0s5elet', 
    }, {
        'txt':  'Hússziget', 
        'brl':  '$h0s5iget', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'hússzínű', 
        'brl':  'h0s5|n}', 
    }, {
        'txt':  'hűvösszemű', 
        'brl':  'h}vqs5em}', 
    }, {
        'txt':  'ideggyengeség', 
        'brl':  'ideg4enges1g', 
    }, {
        'txt':  'ideggyógyászat', 
        'brl':  'ideg494"5at', 
    }, {
        'txt':  'ideggyógyintézet', 
        'brl':  'ideg494int12et', 
    }, {
        'txt':  'ideggyönge', 
        'brl':  'ideg4qnge', 
    }, {
        'txt':  'ideggyötrő', 
        'brl':  'ideg4qtr7', 
    }, {
        'txt':  'ideggyulladás', 
        'brl':  'ideg4ullad"s', 
    }, {
        'txt':  'identitászavar', 
        'brl':  'identit"s2avar', 
    }, {
        'txt':  'időjárásszolgálat', 
        'brl':  'id7j"r"s5olg"lat', 
    }, {
        'txt':  'imádsággyűjtemény', 
        'brl':  'im"ds"g4}jtem16', 
    }, {
        'txt':  'inasszerep', 
        'brl':  'inas5erep', 
    }, {
        'txt':  'inasszerepet ', 
        'brl':  'inas5erepet ', 
    }, {
        'txt':  'inasszeretetet', 
        'brl':  'inas5eretetet', 
    }, {
        'txt':  'indiánnyelv', 
        'brl':  'indi"n6elv', 
    }, {
        'txt':  'ínnyújtó,', 
        'brl':  '|n60jt9,', 
    }, {
        'txt':  'ínnyújtót ', 
        'brl':  '|n60jt9t ', 
    }, {
        'txt':  'ínyencség', 
        'brl':  '|6encs1g', 
    }, {
        'txt':  'írásszeretet,', 
        'brl':  '|r"s5eretet,', 
    }, {
        'txt':  'irtásszél', 
        'brl':  'irt"s51l', 
    }, {
        'txt':  'istennyila', 
        'brl':  'isten6ila', 
    }, {
        'txt':  'járásszerű ', 
        'brl':  'j"r"s5er} ', 
    }, {
        'txt':  'jáspisszobor', 
        'brl':  'j"spis5obor', 
    }, {
        'txt':  'jegeccsoport', 
        'brl':  'jegec3oport', 
    }, {
        'txt':  'jéggyártás', 
        'brl':  'j1g4"rt"s', 
    }, {
        'txt':  'jelenésszerű ', 
        'brl':  'jelen1s5er} ', 
    }, {
        'txt':  'jelentésszerű', 
        'brl':  'jelent1s5er}', 
    }, {
        'txt':  'jelentésszint', 
        'brl':  'jelent1s5int', 
    }, {
        'txt':  'jelzésszerű', 
        'brl':  'jel21s5er}', 
    }, {
        'txt':  'jósszavai,', 
        'brl':  'j9s5avai,', 
    }, {
        'txt':  'jósszelleme,', 
        'brl':  'j9s5elleme,', 
    }, {
        'txt':  'kabinnyíláson', 
        'brl':  'kabin6|l"son', 
    }, {
        'txt':  'kabinnyomás', 
        'brl':  'kabin6om"s', 
    }, {
        'txt':  'kakasszó', 
        'brl':  'kakas59', 
    }, {
        'txt':  'kalapácszengés', 
        'brl':  'kalap"32eng1s', 
    }, {
        'txt':  'kamionnyi', 
        'brl':  'kamion6i', 
    }, {
        'txt':  'kaparásszerű', 
        'brl':  'kapar"s5er}', 
    }, {
        'txt':  'Kaposszekcső', 
        'brl':  '$kapos5ek37', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Kaposszerdahely', 
        'brl':  '$kapos5erdahe', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kapusszoba', 
        'brl':  'kapus5oba', 
    }, {
        'txt':  'karosszék', 
        'brl':  'karos51k', 
    }, {
        'txt':  'kartácszápor', 
        'brl':  'kart"32"por', 
    }, {
        'txt':  'kartonnyi', 
        'brl':  'karton6i', 
    }, {
        'txt':  'kasszék', 
        'brl':  'kas51k', 
    }, {
        'txt':  'katalógusszám', 
        'brl':  'katal9gus5"m', 
    }, {
        'txt':  'katekizmusszerű', 
        'brl':  'kateki2mus5er}', 
    }, {
        'txt':  'kavarásszerű', 
        'brl':  'kavar"s5er}', 
    }, {
        'txt':  'kavicsszerű ', 
        'brl':  'kavi35er} ', 
    }, {
        'txt':  'kavicszápor', 
        'brl':  'kavi32"por', 
    }, {
        'txt':  'kavicszátony', 
        'brl':  'kavi32"to6', 
    }, {
        'txt':  'kavicszuzalék', 
        'brl':  'kavi32u2al1k', 
    }, {
        'txt':  'kékesszürke', 
        'brl':  'k1kes5{rke', 
    }, {
        'txt':  'Kemenesszentmárton', 
        'brl':  '$kemenes5entm"rton', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Kemenesszentpéter', 
        'brl':  '$kemenes5entp1ter', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'képmásszerű ', 
        'brl':  'k1pm"s5er} ', 
    }, {
        'txt':  'képzésszerű ', 
        'brl':  'k1p21s5er} ', 
    }, {
        'txt':  'képzésszervezés', 
        'brl':  'k1p21s5erve21s', 
    }, {
        'txt':  'kerekesszék', 
        'brl':  'kerekes51k', 
    }, {
        'txt':  'keresésszolgáltató', 
        'brl':  'keres1s5olg"ltat9', 
    }, {
        'txt':  'kérésszerűen', 
        'brl':  'k1r1s5er}en', 
    }, {
        'txt':  'kerítésszaggató', 
        'brl':  'ker|t1s5aggat9', 
    }, {
        'txt':  'késszúrás', 
        'brl':  'k1s50r"s', 
    }, {
        'txt':  'kevésszavú ', 
        'brl':  'kev1s5av0 ', 
    }, {
        'txt':  'kevésszer', 
        'brl':  'kev1s5er', 
    }, {
        'txt':  'kifejlesszem', 
        'brl':  'kifejle55em', 
    }, {
        'txt':  'kihívásszerű', 
        'brl':  'kih|v"s5er}', 
    }, {
        'txt':  'kilenccsatorna', 
        'brl':  'kilenc3atorna', 
    }, {
        'txt':  'kilincszörgést ', 
        'brl':  'kilin32qrg1st ', 
    }, {
        'txt':  'Kisszállás', 
        'brl':  '$kis5"ll"s', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kisszámú', 
        'brl':  'kis5"m0', 
    }, {
        'txt':  'Kisszeben', 
        'brl':  '$kis5eben', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kisszék', 
        'brl':  'kis51k', 
    }, {
        'txt':  'kisszekrény', 
        'brl':  'kis5ekr16', 
    }, {
        'txt':  'Kisszentmárton', 
        'brl':  '$kis5entm"rton', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kisszerű', 
        'brl':  'kis5er}', 
    }, {
        'txt':  'Kissziget', 
        'brl':  '$kis5iget', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kisszobában', 
        'brl':  'kis5ob"ban', 
    }, {
        'txt':  'kisszótár', 
        'brl':  'kis59t"r', 
    }, {
        'txt':  'Kiszombor', 
        'brl':  '$kis2ombor', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kiszögellésszerűen', 
        'brl':  'ki5qgell1s5er}en', 
    }, {
        'txt':  'Kiszsidány', 
        'brl':  '$kis`id"6', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kitörésszerű', 
        'brl':  'kitqr1s5er}', 
    }, {
        'txt':  'kitüntetésszalagokat', 
        'brl':  'kit{ntet1s5alagokat', 
    }, {
        'txt':  'kliensszoftver', 
        'brl':  'kliens5oftver', 
    }, {
        'txt':  'kóccsomó', 
        'brl':  'k9c3om9', 
    }, {
        'txt':  'koldusszáj', 
        'brl':  'koldus5"j', 
    }, {
        'txt':  'koldusszakáll', 
        'brl':  'koldus5ak"ll', 
    }, {
        'txt':  'koldusszegény', 
        'brl':  'koldus5eg16', 
    }, {
        'txt':  'kolduszene ', 
        'brl':  'koldus2ene ', 
    }, {
        'txt':  'kommunikációsszoba', 
        'brl':  'kommunik"ci9s5oba', 
    }, {
        'txt':  'komposszesszor', 
        'brl':  'kompo55e55or', 
    }, {
        'txt':  'komposszesszorátus ', 
        'brl':  'kompo55e55or"tus ', 
    }, {
        'txt':  'kormosszürke', 
        'brl':  'kormos5{rke', 
    }, {
        'txt':  'kosszarv', 
        'brl':  'kos5arv', 
    }, {
        'txt':  'kosszem', 
        'brl':  'kos5em', 
    }, {
        'txt':  'köhögésszerű', 
        'brl':  'kqhqg1s5er}', 
    }, {
        'txt':  'kőművesszem', 
        'brl':  'k7m}ves5em', 
    }, {
        'txt':  'kőművesszerszámait ', 
        'brl':  'k7m}ves5er5"mait ', 
    }, {
        'txt':  'köntösszegély', 
        'brl':  'kqntqs5eg1', 
    }, {
        'txt':  'könyöklésszéles', 
        'brl':  'kq6qkl1s51les', 
    }, {
        'txt':  'könyvesszekrény', 
        'brl':  'kq6ves5ekr16', 
    }, {
        'txt':  'kőrisszár', 
        'brl':  'k7ris5"r', 
    }, {
        'txt':  'kőrisszárat ', 
        'brl':  'k7ris5"rat ', 
    }, {
        'txt':  'körösszakál', 
        'brl':  'kqrqs5ak"l', 
    }, {
        'txt':  'körösszakáli ', 
        'brl':  'kqrqs5ak"li ', 
    }, {
        'txt':  'Köröszug', 
        'brl':  '$kqrqs2ug', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'kötelességgyakorlás', 
        'brl':  'kqteless1g4akorl"s', 
    }, {
        'txt':  'közsáv', 
        'brl':  'kq2s"v', 
    }, {
        'txt':  'közsereg', 
        'brl':  'kq2sereg', 
    }, {
        'txt':  'kudarcsorozat', 
        'brl':  'kudarcsoro2at', 
    }, {
        'txt':  'kulcszörgés', 
        'brl':  'kul32qrg1s', 
    }, {
        'txt':  'kuplunggyár', 
        'brl':  'kuplung4"r', 
    }, {
        'txt':  'kurucság', 
        'brl':  'kurucs"g', 
    }, {
        'txt':  'küldetésszaga', 
        'brl':  'k{ldet1s5aga', 
    }, {
        'txt':  'különcség', 
        'brl':  'k{lqncs1g', 
    }, {
        'txt':  'labirintusszerű', 
        'brl':  'labirintus5er}', 
    }, {
        'txt':  'lakásszentelő', 
        'brl':  'lak"s5entel7', 
    }, {
        'txt':  'lakásszövetkezet', 
        'brl':  'lak"s5qvetke2et', 
    }, {
        'txt':  'lakosszám', 
        'brl':  'lakos5"m', 
    }, {
        'txt':  'lánccsörgés ', 
        'brl':  'l"nc3qrg1s ', 
    }, {
        'txt':  'láncszem', 
        'brl':  'l"nc5em', 
    }, {
        'txt':  'lánggyújtogató', 
        'brl':  'l"ng40jtogat9', 
    }, {
        'txt':  'laposszárú', 
        'brl':  'lapos5"r0', 
    }, {
        'txt':  'látásszög', 
        'brl':  'l"t"s5qg', 
    }, {
        'txt':  'látászavar', 
        'brl':  'l"t"s2avar', 
    }, {
        'txt':  'látomásszerű', 
        'brl':  'l"tom"s5er}', 
    }, {
        'txt':  'lazaccsontváz', 
        'brl':  'la2ac3ontv"2', 
    }, {
        'txt':  'lázsebességgel', 
        'brl':  'l"2sebess1ggel', 
    }, {
        'txt':  'lázsóhajtás', 
        'brl':  'l"2s9hajt"s', 
    }, {
        'txt':  'léggyök', 
        'brl':  'l1g4qk', 
    }, {
        'txt':  'léggyökér ', 
        'brl':  'l1g4qk1r ', 
    }, {
        'txt':  'lejtésszög', 
        'brl':  'lejt1s5qg', 
    }, {
        'txt':  'lejtésszöge', 
        'brl':  'lejt1s5qge', 
    }, {
        'txt':  'lemezstúdió', 
        'brl':  'leme2st0di9', 
    }, {
        'txt':  'lengésszabály', 
        'brl':  'leng1s5ab"', 
    }, {
        'txt':  'lépcsősszárnyú', 
        'brl':  'l1p37s5"r60', 
    }, {
        'txt':  'lépésszám', 
        'brl':  'l1p1s5"m', 
    }, {
        'txt':  'lépészaj ', 
        'brl':  'l1p1s2aj ', 
    }, {
        'txt':  'léptennyomon', 
        'brl':  'l1pten6omon', 
    }, {
        'txt':  'levesszedő', 
        'brl':  'leves5ed7', 
    }, {
        'txt':  'liberty', 
        'brl':  'liberty', 
    }, {
        'txt':  'licenccsalád ', 
        'brl':  'licenc3al"d ', 
    }, {
        'txt':  'licencsértés', 
        'brl':  'licencs1rt1s', 
    }, {
        'txt':  'liszteszacskó', 
        'brl':  'li5tes2a3k9', 
    }, {
        'txt':  'loggyűjtemény', 
        'brl':  'log4}jtem16', 
    }, {
        'txt':  'lovasszázad', 
        'brl':  'lovas5"2ad', 
    }, {
        'txt':  'lovasszekeret ', 
        'brl':  'lovas5ekeret ', 
    }, {
        'txt':  'lökésszám', 
        'brl':  'lqk1s5"m', 
    }, {
        'txt':  'lökésszerű ', 
        'brl':  'lqk1s5er} ', 
    }, {
        'txt':  'lőporosszekér', 
        'brl':  'l7poros5ek1r', 
    }, {
        'txt':  'lőporosszekerek', 
        'brl':  'l7poros5ekerek', 
    }, {
        'txt':  'lőrésszerű', 
        'brl':  'l7r1s5er}', 
    }, {
        'txt':  'lugasszerű', 
        'brl':  'lugas5er}', 
    }, {
        'txt':  'luxusszálloda', 
        'brl':  'luxus5"lloda', 
    }, {
        'txt':  'macskakaparásszerű ', 
        'brl':  'ma3kakapar"s5er} ', 
    }, {
        'txt':  'magánnyelvmesterek', 
        'brl':  'mag"n6elvmesterek', 
    }, {
        'txt':  'magánnyugdíjpénztár', 
        'brl':  'mag"n6ugd|jp1n2t"r', 
    }, {
        'txt':  'magasszárú', 
        'brl':  'magas5"r0', 
    }, {
        'txt':  'mágnásszámba', 
        'brl':  'm"gn"s5"mba', 
    }, {
        'txt':  'mágnesszalag', 
        'brl':  'm"gnes5alag', 
    }, {
        'txt':  'mágnesszerű', 
        'brl':  'm"gnes5er}', 
    }, {
        'txt':  'mágneszár', 
        'brl':  'm"gnes2"r', 
    }, {
        'txt':  'malacság', 
        'brl':  'malacs"g', 
    }, {
        'txt':  'malacsült ', 
        'brl':  'malacs{lt ', 
    }, {
        'txt':  'málhásszekér', 
        'brl':  'm"lh"s5ek1r', 
    }, {
        'txt':  'málhásszekereiket ', 
        'brl':  'm"lh"s5ekereiket ', 
    }, {
        'txt':  'mandarinnyelv', 
        'brl':  'mandarin6elv', 
    }, {
        'txt':  'Marosszék', 
        'brl':  '$maros51k', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Marosszentgyörgy ', 
        'brl':  '$maros5ent4qr4 ', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Maroszug', 
        'brl':  '$maros2ug', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Martraversszal', 
        'brl':  '$martraver55al', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'másszínű', 
        'brl':  'm"s5|n}', 
    }, {
        'txt':  'másszor', 
        'brl':  'm"s5or', 
    }, {
        'txt':  'másszóval', 
        'brl':  'm"s59val', 
    }, {
        'txt':  'másszőrűek', 
        'brl':  'm"s57r}ek', 
    }, {
        'txt':  'mécsesszem', 
        'brl':  'm13es5em', 
    }, {
        'txt':  'medresszék', 
        'brl':  'medres51k', 
    }, {
        'txt':  'meggybefőtt', 
        'brl':  'me44bef7tt', 
    }, {
        'txt':  'meggyel', 
        'brl':  'me44el', 
    }, {
        'txt':  'meggyen', 
        'brl':  'me44en', 
    }, {
        'txt':  'meggyes', 
        'brl':  'me44es', 
    }, {
        'txt':  'Meggyesi', 
        'brl':  '$me44esi', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'meggyet', 
        'brl':  'me44et', 
    }, {
        'txt':  'meggyfa', 
        'brl':  'me44fa', 
    }, {
        'txt':  'meggyhez', 
        'brl':  'me44he2', 
    }, {
        'txt':  'meggyízű', 
        'brl':  'me44|2}', 
    }, {
        'txt':  'meggylekvár', 
        'brl':  'me44lekv"r', 
    }, {
        'txt':  'meggymag', 
        'brl':  'me44mag', 
    }, {
        'txt':  'meggynek', 
        'brl':  'me44nek', 
    }, {
        'txt':  'meggypiros', 
        'brl':  'me44piros', 
    }, {
        'txt':  'meggyszín', 
        'brl':  'me445|n', 
    }, {
        'txt':  'meggytől', 
        'brl':  'me44t7l', 
    }, {
        'txt':  'meggytől', 
        'brl':  'me44t7l', 
    }, {
        'txt':  'meggyvörös', 
        'brl':  'me44vqrqs', 
    }, {
        'txt':  'méhesszín', 
        'brl':  'm1hes5|n', 
    }, {
        'txt':  'méhesszínben ', 
        'brl':  'm1hes5|nben ', 
    }, {
        'txt':  'ménesszárnyékok', 
        'brl':  'm1nes5"r61kok', 
    }, {
        'txt':  'méreggyökeret', 
        'brl':  'm1reg4qkeret', 
    }, {
        'txt':  'méreggyökérré', 
        'brl':  'm1reg4qk1rr1', 
    }, {
        'txt':  'méreggyümölccsé ', 
        'brl':  'm1reg4{mql331 ', 
    }, {
        'txt':  'mérleggyár', 
        'brl':  'm1rleg4"r', 
    }, {
        'txt':  'meszesszürke', 
        'brl':  'me5es5{rke', 
    }, {
        'txt':  'mézsárga', 
        'brl':  'm12s"rga', 
    }, {
        'txt':  'mézser', 
        'brl':  'm12ser', 
    }, {
        'txt':  'mézsör', 
        'brl':  'm12sqr', 
    }, {
        'txt':  'Mikosszéplak', 
        'brl':  '$mikos51plak', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'mikrofonnyílás', 
        'brl':  'mikrofon6|l"s', 
    }, {
        'txt':  'mikronnyi', 
        'brl':  'mikron6i', 
    }, {
        'txt':  'mitesszer', 
        'brl':  'mite55er', 
    }, {
        'txt':  'mohaszöld', 
        'brl':  'mohas2qld', 
    }, {
        'txt':  'mókusszerű ', 
        'brl':  'm9kus5er} ', 
    }, {
        'txt':  'mókusszőr', 
        'brl':  'm9kus57r', 
    }, {
        'txt':  'motorosszán', 
        'brl':  'motoros5"n', 
    }, {
        'txt':  'mozgásszerű', 
        'brl':  'mo2g"s5er}', 
    }, {
        'txt':  'mozgászavar', 
        'brl':  'mo2g"s2avar', 
    }, {
        'txt':  'munkásszálló', 
        'brl':  'munk"s5"ll9', 
    }, {
        'txt':  'muzsikusszem', 
        'brl':  'mu`ikus5em', 
    }, {
        'txt':  'műjéggyár', 
        'brl':  'm}j1g4"r', 
    }, {
        'txt':  'működészavar ', 
        'brl':  'm}kqd1s2avar ', 
    }, {
        'txt':  'nászutazásszerű ', 
        'brl':  'n"5uta2"s5er} ', 
    }, {
        'txt':  'nedvesszürke', 
        'brl':  'nedves5{rke', 
    }, {
        'txt':  'négyzetyard', 
        'brl':  'n142etyard', 
    }, {
        'txt':  'nehézség', 
        'brl':  'neh12s1g', 
    }, {
        'txt':  'nehézsúlyú', 
        'brl':  'neh12s00', 
    }, {
        'txt':  'Nemesszalók', 
        'brl':  '$nemes5al9k', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Nemesszentandrás', 
        'brl':  '$nemes5entandr"s', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'nemesszőrme', 
        'brl':  'nemes57rme', 
    }, {
        'txt':  'nemezsapka', 
        'brl':  'neme2sapka', 
    }, {
        'txt':  'nemezsapkát', 
        'brl':  'neme2sapk"t', 
    }, {
        'txt':  'nemezsátor ', 
        'brl':  'neme2s"tor ', 
    }, {
        'txt':  'nercstóla', 
        'brl':  'nercst9la', 
    }, {
        'txt':  'nyargonccsizma', 
        'brl':  '6argonc3i2ma', 
    }, {
        'txt':  'nyereggyártók', 
        'brl':  '6ereg4"rt9k', 
    }, {
        'txt':  'nyikkanásszerű', 
        'brl':  '6ikkan"s5er}', 
    }, {
        'txt':  'nyílászáró', 
        'brl':  '6|l"s2"r9', 
    }, {
        'txt':  'nyolcvannyolc', 
        'brl':  '6olcvan6olc', 
    }, {
        'txt':  'nyolccsatornás ', 
        'brl':  '6olc3atorn"s ', 
    }, {
        'txt':  'nyolcsebességes', 
        'brl':  '6olcsebess1ges', 
    }, {
        'txt':  'nyomásszabályzó', 
        'brl':  '6om"s5ab"29', 
    }, {
        'txt':  'nyomásszerű', 
        'brl':  '6om"s5er}', 
    }, {
        'txt':  'ordasszőrű', 
        'brl':  'ordas57r}', 
    }, {
        'txt':  'óriásszalamandra', 
        'brl':  '9ri"s5alamandra', 
    }, {
        'txt':  'oroszlánnyom', 
        'brl':  'oro5l"n6om', 
    }, {
        'txt':  'orvosszakértő', 
        'brl':  'orvos5ak1rt7', 
    }, {
        'txt':  'orvosszázados', 
        'brl':  'orvos5"2ados', 
    }, {
        'txt':  'orvosszemély', 
        'brl':  'orvos5em1', 
    }, {
        'txt':  'orvosszereikkel', 
        'brl':  'orvos5ereikkel', 
    }, {
        'txt':  'orvosszerű', 
        'brl':  'orvos5er}', 
    }, {
        'txt':  'orvosszövetség', 
        'brl':  'orvos5qvets1g', 
    }, {
        'txt':  'óvodásszintű', 
        'brl':  '9vod"s5int}', 
    }, {
        'txt':  'ökrösszekér', 
        'brl':  'qkrqs5ek1r', 
    }, {
        'txt':  'önállásszerű', 
        'brl':  'qn"ll"s5er}', 
    }, {
        'txt':  'önnyomása,', 
        'brl':  'qn6om"sa,', 
    }, {
        'txt':  'önnyomatú ', 
        'brl':  'qn6omat0 ', 
    }, {
        'txt':  'örvénylésszerű', 
        'brl':  'qrv16l1s5er}', 
    }, {
        'txt':  'ősszármazású ', 
        'brl':  '7s5"rma2"s0 ', 
    }, {
        'txt':  'ősszékelyek', 
        'brl':  '7s51keek', 
    }, {
        'txt':  'összekéregetett', 
        'brl':  'q55ek1regetett', 
    }, {
        'txt':  'összekéregettek', 
        'brl':  'q55ek1regettek', 
    }, {
        'txt':  'összekeresgélt', 
        'brl':  'q55ekeresg1lt', 
    }, {
        'txt':  'ősszel', 
        'brl':  '755el', 
    }, {
        'txt':  'ősszellem', 
        'brl':  '7s5ellem', 
    }, {
        'txt':  'őzsebesen', 
        'brl':  '72sebesen', 
    }, {
        'txt':  'őzsörét', 
        'brl':  '72sqr1t', 
    }, {
        'txt':  'őzsuta', 
        'brl':  '72suta', 
    }, {
        'txt':  'padlásszoba', 
        'brl':  'padl"s5oba', 
    }, {
        'txt':  'padlászugoly', 
        'brl':  'padl"s2ugo', 
    }, {
        'txt':  'palócság', 
        'brl':  'pal9cs"g', 
    }, {
        'txt':  'papirosszeletre', 
        'brl':  'papiros5eletre', 
    }, {
        'txt':  'paprikásszalonna-bazár', 
        'brl':  'paprik"s5alonna-ba2"r', 
    }, {
        'txt':  'papucszápor', 
        'brl':  'papu32"por', 
    }, {
        'txt':  'párnásszék', 
        'brl':  'p"rn"s51k', 
    }, {
        'txt':  'pátosszal', 
        'brl':  'p"to55al', 
    }, {
        'txt':  'pedagógusszervezet', 
        'brl':  'pedag9gus5erve2et', 
    }, {
        'txt':  'pedagógusszobába', 
        'brl':  'pedag9gus5ob"ba', 
    }, {
        'txt':  'pedagógussztrájk', 
        'brl':  'pedag9gus5tr"jk', 
    }, {
        'txt':  'pénzeszacskó', 
        'brl':  'p1n2es2a3k9', 
    }, {
        'txt':  'pénzeszsák', 
        'brl':  'p1n2es`"k', 
    }, {
        'txt':  'pénzzsidóságban ', 
        'brl':  'p1n2`id9s"gban ', 
    }, {
        'txt':  'pénzsegély', 
        'brl':  'p1n2seg1', 
    }, {
        'txt':  'pénzsorozat', 
        'brl':  'p1n2soro2at', 
    }, {
        'txt':  'pénzsóvár', 
        'brl':  'p1n2s9v"r', 
    }, {
        'txt':  'pénzszűke', 
        'brl':  'p1n25}ke', 
    }, {
        'txt':  'perecsütőnő', 
        'brl':  'perecs{t7n7', 
    }, {
        'txt':  'pergamennyaláb', 
        'brl':  'pergamen6al"b', 
    }, {
        'txt':  'piaccsarnok', 
        'brl':  'piac3arnok', 
    }, {
        'txt':  'Pilisszántó,', 
        'brl':  '$pilis5"nt9,', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Pilisszentkereszt', 
        'brl':  '$pilis5entkere5t', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'pirítósszeleteken', 
        'brl':  'pir|t9s5eleteken', 
    }, {
        'txt':  'pirosszem', 
        'brl':  'piros5em', 
    }, {
        'txt':  'piroszászlós', 
        'brl':  'piros2"5l9s', 
    }, {
        'txt':  'piszkosszőke', 
        'brl':  'pi5kos57ke', 
    }, {
        'txt':  'piszkosszürke ', 
        'brl':  'pi5kos5{rke ', 
    }, {
        'txt':  'piszkoszöld', 
        'brl':  'pi5kos2qld', 
    }, {
        'txt':  'pisztolylövésszerűen', 
        'brl':  'pi5tolqv1s5er}en', 
    }, {
        'txt':  'plüsszerű', 
        'brl':  'pl{s5er}', 
    }, {
        'txt':  'plüsszsák', 
        'brl':  'pl{ss`"k', 
    }, {
        'txt':  'plüsszsiráf ', 
        'brl':  'pl{ss`ir"f ', 
    }, {
        'txt':  'plüsszsölyét', 
        'brl':  'pl{ss`q1t', 
    }, {
        'txt':  'pokróccsík', 
        'brl':  'pokr9c3|k', 
    }, {
        'txt':  'polcsor', 
        'brl':  'polcsor', 
    }, {
        'txt':  'policájkomisszér', 
        'brl':  'polic"jkomi551r', 
    }, {
        'txt':  'porcsérülés', 
        'brl':  'porcs1r{l1s', 
    }, {
        'txt':  'portásszoba', 
        'brl':  'port"s5oba', 
    }, {
        'txt':  'postásszakszervezet', 
        'brl':  'post"s5ak5erve2et', 
    }, {
        'txt':  'precízség', 
        'brl':  'prec|2s1g', 
    }, {
        'txt':  'présszerű', 
        'brl':  'pr1s5er}', 
    }, {
        'txt':  'priamossza', 
        'brl':  'priamo55a', 
    }, {
        'txt':  'princséged', 
        'brl':  'princs1ged', 
    }, {
        'txt':  'proggyak', 
        'brl':  'prog4ak', 
    }, {
        'txt':  'pulzusszám', 
        'brl':  'pul2us5"m', 
    }, {
        'txt':  'puskásszakasz', 
        'brl':  'pusk"s5aka5', 
    }, {
        'txt':  'puskásszázad', 
        'brl':  'pusk"s5"2ad', 
    }, {
        'txt':  'puskászászlóalj ', 
        'brl':  'pusk"s2"5l9alj ', 
    }, {
        'txt':  'rácság', 
        'brl':  'r"cs"g', 
    }, {
        'txt':  'rádiósszoba', 
        'brl':  'r"di9s5oba', 
    }, {
        'txt':  'rakásszámra', 
        'brl':  'rak"s5"mra', 
    }, {
        'txt':  'rándulásszerűen', 
        'brl':  'r"ndul"s5er}en', 
    }, {
        'txt':  'rántásszag', 
        'brl':  'r"nt"s5ag', 
    }, {
        'txt':  'rántásszerű ', 
        'brl':  'r"nt"s5er} ', 
    }, {
        'txt':  'recésszárú', 
        'brl':  'rec1s5"r0', 
    }, {
        'txt':  'régiséggyűjtő', 
        'brl':  'r1gis1g4}jt7', 
    }, {
        'txt':  'rémisszétek', 
        'brl':  'r1mi551tek', 
    }, {
        'txt':  'rendelkezésszerű', 
        'brl':  'rendelke21s5er}', 
    }, {
        'txt':  'repülésszerű', 
        'brl':  'rep{l1s5er}', 
    }, {
        'txt':  'repülősszárny', 
        'brl':  'rep{l7s5"r6', 
    }, {
        'txt':  'rézsut', 
        'brl':  'r1`ut', 
    }, {
        'txt':  'ritkasággyűjtő', 
        'brl':  'ritkas"g4}jt7', 
    }, {
        'txt':  'ritmusszabályozó', 
        'brl':  'ritmus5ab"o29', 
    }, {
        'txt':  'ritmuszavar', 
        'brl':  'ritmus2avar', 
    }, {
        'txt':  'robbanásszerű', 
        'brl':  'robban"s5er}', 
    }, {
        'txt':  'robbanászajt', 
        'brl':  'robban"s2ajt', 
    }, {
        'txt':  'rongyosszélű', 
        'brl':  'ron4os51l}', 
    }, {
        'txt':  'rosszemlékű', 
        'brl':  'ro55eml1k}', 
    }, {
        'txt':  'rothadásszag', 
        'brl':  'rothad"s5ag', 
    }, {
        'txt':  'rózsásszőkés', 
        'brl':  'r9`"s57k1s', 
    }, {
        'txt':  'rozszabálás', 
        'brl':  'ro`2ab"l"s', 
    }, {
        'txt':  'rubinnyaklánc', 
        'brl':  'rubin6akl"nc', 
    }, {
        'txt':  'ruhásszekrény', 
        'brl':  'ruh"s5ekr16', 
    }, {
        'txt':  'ruhásszobámé', 
        'brl':  'ruh"s5ob"m1', 
    }, {
        'txt':  'sárgásszínű', 
        'brl':  's"rg"s5|n}', 
    }, {
        'txt':  'sasszárnyú', 
        'brl':  'sas5"r60', 
    }, {
        'txt':  'sasszeg', 
        'brl':  'sas5eg', 
    }, {
        'txt':  'sásszéna', 
        'brl':  's"s51na', 
    }, {
        'txt':  'sásszerű ', 
        'brl':  's"s5er} ', 
    }, {
        'txt':  'sasszömöd', 
        'brl':  'sas5qmqd', 
    }, {
        'txt':  'sátáncsapat', 
        'brl':  's"t"n3apat', 
    }, {
        'txt':  'selymesszőke', 
        'brl':  'semes57ke', 
    }, {
        'txt':  'sereggyűjtés', 
        'brl':  'sereg4}jt1s', 
    }, {
        'txt':  'sertésszelet', 
        'brl':  'sert1s5elet', 
    }, {
        'txt':  'sertésszűzpecsenyére', 
        'brl':  'sert1s5}2pe3e61re', 
    }, {
        'txt':  'sertészsír', 
        'brl':  'sert1s`|r', 
    }, {
        'txt':  'sonkásszelet', 
        'brl':  'sonk"s5elet', 
    }, {
        'txt':  'sonkásszerű ', 
        'brl':  'sonk"s5er} ', 
    }, {
        'txt':  'sonkászsemle', 
        'brl':  'sonk"s`emle', 
    }, {
        'txt':  'sorscsapásszerű,', 
        'brl':  'sors3ap"s5er},', 
    }, {
        'txt':  'spanyolmeggyen', 
        'brl':  'spa6olme44en', 
    }, {
        'txt':  'sugárzásszerű ', 
        'brl':  'sug"r2"s5er} ', 
    }, {
        'txt':  'sugárzásszintek', 
        'brl':  'sug"r2"s5intek', 
    }, {
        'txt':  'suhanccsürhe', 
        'brl':  'suhanc3{rhe', 
    }, {
        'txt':  'suhancsereg', 
        'brl':  'suhancsereg', 
    }, {
        'txt':  'szaglásszék', 
        'brl':  '5agl"551k', 
    }, {
        'txt':  'szalonnyelv, szalonnyelven ', 
        'brl':  '5alon6elv, 5alon6elven ', 
    }, {
        'txt':  'Szamosszeg', 
        'brl':  '$5amos5eg', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'szanitéccsoport', 
        'brl':  '5anit1c3oport', 
    }, {
        'txt':  'szárazság', 
        'brl':  '5"ra2s"g', 
    }, {
        'txt':  'szárazsült', 
        'brl':  '5"ra2s{lt', 
    }, {
        'txt':  'szarvasszív', 
        'brl':  '5arvas5|v', 
    }, {
        'txt':  'szarvaszsír', 
        'brl':  '5arvas`|r', 
    }, {
        'txt':  'szénásszekér', 
        'brl':  '51n"s5ek1r', 
    }, {
        'txt':  'szentségesszűzmáriám', 
        'brl':  '5ents1ges5}2m"ri"m', 
    }, {
        'txt':  'szentséggyalázás', 
        'brl':  '5ents1g4al"2"s', 
    }, {
        'txt':  'Szepesszombat', 
        'brl':  '$5epes5ombat', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'szerződésszegés', 
        'brl':  '5er27d1s5eg1s', 
    }, {
        'txt':  'szerződésszegő ', 
        'brl':  '5er27d1s5eg7 ', 
    }, {
        'txt':  'szétossza', 
        'brl':  '51to55a', 
    }, {
        'txt':  'szétosszák ', 
        'brl':  '51to55"k ', 
    }, {
        'txt':  'Szilvásszentmárton', 
        'brl':  '$5ilv"s5entm"rton', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'szindikátusszerű ', 
        'brl':  '5indik"tus5er} ', 
    }, {
        'txt':  'szindikátusszervező', 
        'brl':  '5indik"tus5erve27', 
    }, {
        'txt':  'színnyomás', 
        'brl':  '5|n6om"s', 
    }, {
        'txt':  'szocsegély', 
        'brl':  '5ocseg1', 
    }, {
        'txt':  'szódásszifon', 
        'brl':  '59d"s5ifon', 
    }, {
        'txt':  'szokásszerű', 
        'brl':  '5ok"s5er}', 
    }, {
        'txt':  'szólásszabadság', 
        'brl':  '59l"s5abads"g', 
    }, {
        'txt':  'szólásszapulás', 
        'brl':  '59l"s5apul"s', 
    }, {
        'txt':  'szórakozásszámba', 
        'brl':  '59rako2"s5"mba', 
    }, {
        'txt':  'szorongásszerű', 
        'brl':  '5orong"s5er}', 
    }, {
        'txt':  'szorongásszint', 
        'brl':  '5orong"s5int', 
    }, {
        'txt':  'szöggyár', 
        'brl':  '5qg4"r', 
    }, {
        'txt':  'szöveggyűjtemény', 
        'brl':  '5qveg4}jtem16', 
    }, {
        'txt':  'szúrásszerű', 
        'brl':  '50r"s5er}', 
    }, {
        'txt':  'szúrósszemű', 
        'brl':  '50r9s5em}', 
    }, {
        'txt':  'születésszabályozás', 
        'brl':  '5{let1s5ab"o2"s', 
    }, {
        'txt':  'szűzsült', 
        'brl':  '5}2s{lt', 
    }, {
        'txt':  'tágulásszabály', 
        'brl':  't"gul"s5ab"', 
    }, {
        'txt':  'taggyűlés', 
        'brl':  'tag4}l1s', 
    }, {
        'txt':  'tánccsoport', 
        'brl':  't"nc3oport', 
    }, {
        'txt':  'társalgásszámba', 
        'brl':  't"rsalg"s5"mba', 
    }, {
        'txt':  'társalgásszerű ', 
        'brl':  't"rsalg"s5er} ', 
    }, {
        'txt':  'társszekér', 
        'brl':  't"rs5ek1r', 
    }, {
        'txt':  'társszövetség', 
        'brl':  't"rs5qvets1g', 
    }, {
        'txt':  'teásszervíz', 
        'brl':  'te"s5erv|2', 
    }, {
        'txt':  'tejfölösszájú', 
        'brl':  'tejfqlqs5"j0', 
    }, {
        'txt':  'településszerkezet ', 
        'brl':  'telep{l1s5erke2et ', 
    }, {
        'txt':  'templomosszakértő', 
        'brl':  'templomos5ak1rt7', 
    }, {
        'txt':  'tetszészaj', 
        'brl':  'tet51s2aj', 
    }, {
        'txt':  'tetszészsivaj ', 
        'brl':  'tet51s`ivaj ', 
    }, {
        'txt':  'tigrisszemek ', 
        'brl':  'tigris5emek ', 
    }, {
        'txt':  'tigrisszerű', 
        'brl':  'tigris5er}', 
    }, {
        'txt':  'tipusszám', 
        'brl':  'tipus5"m', 
    }, {
        'txt':  'típusszám', 
        'brl':  't|pus5"m', 
    }, {
        'txt':  'típusszöveg', 
        'brl':  't|pus5qveg', 
    }, {
        'txt':  'típuszubbony', 
        'brl':  't|pus2ubbo6', 
    }, {
        'txt':  'titkosszolgálat', 
        'brl':  'titkos5olg"lat', 
    }, {
        'txt':  'tizedesszállás', 
        'brl':  'ti2edes5"ll"s', 
    }, {
        'txt':  'tízgallonnyi', 
        'brl':  't|2gallon6i', 
    }, {
        'txt':  'tojásszerű', 
        'brl':  'toj"s5er}', 
    }, {
        'txt':  'topázsárgája', 
        'brl':  'top"2s"rg"ja', 
    }, {
        'txt':  'torzság', 
        'brl':  'tor2s"g', 
    }, {
        'txt':  'többesszám', 
        'brl':  'tqbbes5"m', 
    }, {
        'txt':  'töltésszabályozó', 
        'brl':  'tqlt1s5ab"o29', 
    }, {
        'txt':  'töltésszám', 
        'brl':  'tqlt1s5"m', 
    }, {
        'txt':  'töltésszerűen', 
        'brl':  'tqlt1s5er}en', 
    }, {
        'txt':  'törlesszem', 
        'brl':  'tqrle55em', 
    }, {
        'txt':  'tövisszár', 
        'brl':  'tqvis5"r', 
    }, {
        'txt':  'tövisszúrás', 
        'brl':  'tqvis50r"s', 
    }, {
        'txt':  'trichinnyavalya', 
        'brl':  'trichin6avaa', 
    }, {
        'txt':  'tudásszint', 
        'brl':  'tud"s5int', 
    }, {
        'txt':  'tudásszomj', 
        'brl':  'tud"s5omj', 
    }, {
        'txt':  'tükrösszélű', 
        'brl':  't{krqs51l}', 
    }, {
        'txt':  'tüntetésszerűen', 
        'brl':  't{ntet1s5er}en', 
    }, {
        'txt':  'tüzesszemű', 
        'brl':  't{2es5em}', 
    }, {
        'txt':  'tűzzsonglőr ', 
        'brl':  't}2`ongl7r ', 
    }, {
        'txt':  'tűzsebesség', 
        'brl':  't}2sebess1g', 
    }, {
        'txt':  'tűzsugár', 
        'brl':  't}2sug"r', 
    }, {
        'txt':  'udvaroncsereg', 
        'brl':  'udvaroncsereg', 
    }, {
        'txt':  'ugrásszerűen', 
        'brl':  'ugr"s5er}en', 
    }, {
        'txt':  'újoncság', 
        'brl':  '0joncs"g', 
    }, {
        'txt':  'ulánusszázad', 
        'brl':  'ul"nus5"2ad', 
    }, {
        'txt':  'university', 
        'brl':  'university', 
    }, {
        'txt':  'utalásszerűen', 
        'brl':  'utal"s5er}en', 
    }, {
        'txt':  'utánpótlásszállítmánnyal', 
        'brl':  'ut"np9tl"s5"ll|tm"66al', 
    }, {
        'txt':  'utánnyomás', 
        'brl':  'ut"n6om"s', 
    }, {
        'txt':  'utasszállító', 
        'brl':  'utas5"ll|t9', 
    }, {
        'txt':  'utasszám', 
        'brl':  'utas5"m', 
    }, {
        'txt':  'utasszerep', 
        'brl':  'utas5erep', 
    }, {
        'txt':  'utasszint', 
        'brl':  'utas5int', 
    }, {
        'txt':  'utasszolgálat', 
        'brl':  'utas5olg"lat', 
    }, {
        'txt':  'utazásszerű', 
        'brl':  'uta2"s5er}', 
    }, {
        'txt':  'utazásszervező', 
        'brl':  'uta2"s5erve27', 
    }, {
        'txt':  'ülésszak', 
        'brl':  '{l1s5ak', 
    }, {
        'txt':  'ülésszak', 
        'brl':  '{l1s5ak', 
    }, {
        'txt':  'ütésszerű', 
        'brl':  '{t1s5er}', 
    }, {
        'txt':  'ütészápor ', 
        'brl':  '{t1s2"por ', 
    }, {
        'txt':  'üveggyapot', 
        'brl':  '{veg4apot', 
    }, {
        'txt':  'üveggyártás', 
        'brl':  '{veg4"rt"s', 
    }, {
        'txt':  'üveggyöngy', 
        'brl':  '{veg4qn4', 
    }, {
        'txt':  'üveggyűrű', 
        'brl':  '{veg4}r}', 
    }, {
        'txt':  'vagonnyi', 
        'brl':  'vagon6i', 
    }, {
        'txt':  'vakarccsal', 
        'brl':  'vakar33al', 
    }, {
        'txt':  'vallásszabadság', 
        'brl':  'vall"s5abads"g', 
    }, {
        'txt':  'vallomásszámba', 
        'brl':  'vallom"s5"mba', 
    }, {
        'txt':  'vállpereccsontjából', 
        'brl':  'v"llperec3ontj"b9l', 
    }, {
        'txt':  'Vámosszabadi', 
        'brl':  '$v"mos5abadi', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'városszépe', 
        'brl':  'v"ros51pe', 
    }, {
        'txt':  'városszerte', 
        'brl':  'v"ros5erte', 
    }, {
        'txt':  'várossziluett', 
        'brl':  'v"ros5iluett', 
    }, {
        'txt':  'városzajon', 
        'brl':  'v"ros2ajon', 
    }, {
        'txt':  'városzajt ', 
        'brl':  'v"ros2ajt ', 
    }, {
        'txt':  'városzsarnok', 
        'brl':  'v"ros`arnok', 
    }, {
        'txt':  'vasasszakosztály', 
        'brl':  'vasas5ako5t"', 
    }, {
        'txt':  'vasutassztrájk', 
        'brl':  'vasutas5tr"jk', 
    }, {
        'txt':  'vasszállítmány', 
        'brl':  'vas5"ll|tm"6', 
    }, {
        'txt':  'Vaszar', 
        'brl':  '$va5ar', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Vázsnok', 
        'brl':  '$v"`nok', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'végiggyakorolni', 
        'brl':  'v1gig4akorolni', 
    }, {
        'txt':  'végiggyalogolhatja', 
        'brl':  'v1gig4alogolhatja', 
    }, {
        'txt':  'vendéggyermek', 
        'brl':  'vend1g4ermek', 
    }, {
        'txt':  'vendéggyülekezet', 
        'brl':  'vend1g4{leke2et', 
    }, {
        'txt':  'veresszakállú ', 
        'brl':  'veres5ak"ll0 ', 
    }, {
        'txt':  'veresszemű', 
        'brl':  'veres5em}', 
    }, {
        'txt':  'versszak', 
        'brl':  'vers5ak', 
    }, {
        'txt':  'versszakában ', 
        'brl':  'vers5ak"ban ', 
    }, {
        'txt':  'versszerű', 
        'brl':  'vers5er}', 
    }, {
        'txt':  'vértesszázad', 
        'brl':  'v1rtes5"2ad', 
    }, {
        'txt':  'vértesszázadbeli ', 
        'brl':  'v1rtes5"2adbeli ', 
    }, {
        'txt':  'Vértesszőlős', 
        'brl':  '$v1rtes57l7s', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'vetésszalag', 
        'brl':  'vet1s5alag', 
    }, {
        'txt':  'viccsor', 
        'brl':  'viccsor', 
    }, {
        'txt':  'világossággyújtásra', 
        'brl':  'vil"goss"g40jt"sra', 
    }, {
        'txt':  'világosszőke', 
        'brl':  'vil"gos57ke', 
    }, {
        'txt':  'világosszürke ', 
        'brl':  'vil"gos5{rke ', 
    }, {
        'txt':  'világgyűlölő', 
        'brl':  'vil"g4}lql7', 
    }, {
        'txt':  'villamosszék', 
        'brl':  'villamos51k', 
    }, {
        'txt':  'villamosszerű', 
        'brl':  'villamos5er}', 
    }, {
        'txt':  'villanásszerűen', 
        'brl':  'villan"s5er}en', 
    }, {
        'txt':  'virággyapjaikat', 
        'brl':  'vir"g4apjaikat', 
    }, {
        'txt':  'virággyűjtemény ', 
        'brl':  'vir"g4}jtem16 ', 
    }, {
        'txt':  'virággyűjtés', 
        'brl':  'vir"g4}jt1s', 
    }, {
        'txt':  'virgoncság', 
        'brl':  'virgoncs"g', 
    }, {
        'txt':  'vírusszerű', 
        'brl':  'v|rus5er}', 
    }, {
        'txt':  'viselkedésszerű', 
        'brl':  'viselked1s5er}', 
    }, {
        'txt':  'viselkedészavar', 
        'brl':  'viselked1s2avar', 
    }, {
        'txt':  'visually', 
        'brl':  'visually', 
    }, {
        'txt':  'visszér', 
        'brl':  'vi551r', 
    }, {
        'txt':  'visszeres', 
        'brl':  'vi55eres', 
    }, {
        'txt':  'visszérműtét ', 
        'brl':  'vi551rm}t1t ', 
    }, {
        'txt':  'vitorlásszezon', 
        'brl':  'vitorl"s5e2on', 
    }, {
        'txt':  'vizeleteszacskó ', 
        'brl':  'vi2eletes2a3k9 ', 
    }, {
        'txt':  'vizeszsemle', 
        'brl':  'vi2es`emle', 
    }, {
        'txt':  'vízsáv', 
        'brl':  'v|2s"v', 
    }, {
        'txt':  'vízsivatag ', 
        'brl':  'v|2sivatag ', 
    }, {
        'txt':  'vízsodor', 
        'brl':  'v|2sodor', 
    }, {
        'txt':  'vonósszerenád ', 
        'brl':  'von9s5eren"d ', 
    }, {
        'txt':  'vonószenekar', 
        'brl':  'von9s2enekar', 
    }, {
        'txt':  'vörhenyesszőke', 
        'brl':  'vqrhe6es57ke', 
    }, {
        'txt':  'vörösesszőke', 
        'brl':  'vqrqses57ke', 
    }, {
        'txt':  'vörösesszőke', 
        'brl':  'vqrqses57ke', 
    }, {
        'txt':  'vörösszakállú', 
        'brl':  'vqrqs5ak"ll0', 
    }, {
        'txt':  'vörösszem', 
        'brl':  'vqrqs5em', 
    }, {
        'txt':  'vörösszínű', 
        'brl':  'vqrqs5|n}', 
    }, {
        'txt':  'vörösszőke', 
        'brl':  'vqrqs57ke', 
    }, {
        'txt':  'vöröszászló', 
        'brl':  'vqrqs2"5l9', 
    }, {
        'txt':  'záloggyűrűmet', 
        'brl':  '2"log4}r}met', 
    }, {
        'txt':  'zavarosszürke', 
        'brl':  '2avaros5{rke', 
    }, {
        'txt':  'zökkenésszerű', 
        'brl':  '2qkken1s5er}', 
    }, {
        'txt':  'zörgésszerű', 
        'brl':  '2qrg1s5er}', 
    }, {
        'txt':  'zuhanásszerű', 
        'brl':  '2uhan"s5er}', 
    }, {
        'txt':  'zsírosszén', 
        'brl':  '`|ros51n', 
    }, {
        'txt':  'zsoldosszokás', 
        'brl':  '`oldos5ok"s', 
    },
    #Testing a sentence with a phone number. Braille output should have numberSign after dashes.
    {
        'txt':  'A telefonszámom: 06-1-256-256.', 
        'brl':  '$a telefon5"mom: #jf-#a-#bef-#bef.', 
        'BRLCursorPos': 1,
    },
    #Testing a decimal number containing sentence. After the comma character not need have numbersign indicator.
    {
        'txt':  'A földrengés 7,5 erősségű volt.', 
        'brl':  '$a fqldreng1s #g,e er7ss1g} volt.', 
        'BRLCursorPos': 1,
    }, {
        'txt':  'Tamás még nem tudta, hogy mi vár rá.', 
        'brl':  '$tam"s m1g nem tudta, ho4 mi v"r r".', 
        'BRLCursorPos': 1,
    },
    #Testing a Quotation mark containing sentence. Two quotation mark characters need resulting different dot combinations.
    #Left quotation mark dot combination is 236, right quotation mark dot combination is 356.
    {
        'txt':  '"Tamás még nem tudta, hogy mi vár rá."', 
        'brl':  '($tam"s m1g nem tudta, ho4 mi v"r r".)', 
    },
    #Testing a sentance containing an apostrophe. The right dot combination is dots 6-3.
    {
        'txt':  "I don't no why happened this problem.", 
        'brl':  "$i don'.t no why happened this problem.", 
        'BRLCursorPos': 1,
    },
    #Testing a § (paragraph mark character) containing sentence. The paragraph mark character dot combination with hungarian language is 3456-1236.
    {
        'txt':  'Az 1§ paragrafus alapján a kedvezmény igénybe vehető.', 
        'brl':  '$a2 #a#v paragrafus alapj"n a kedve2m16 ig16be vehet7.', 
        'BRLCursorPos': 1,
    },
    #testing some sentences with containing – character from a book.
    {
        'txt':  '– A sivatagi karavánúton, negyven vagy ötven mérföldnyire innen. Egy Ford. De mi nem megyünk magával. – Elővette az irattárcáját, és átadott Naszifnak egy angol fontot. – Ha visszajön, keressen meg a vasútállomás mellett a Grand Hotelban.', 
        'brl':  '- $a sivatagi karav"n0ton, ne4ven va4 qtven m1rfqld6ire innen. $e4 $ford. $de mi nem me4{nk mag"val. - $el7vette a2 iratt"rc"j"t, 1s "tadott $na5ifnak e4 angol fontot. - $ha vi55ajqn, keressen meg a vas0t"llom"s mellett a $grand $hotelban.', 
    },
    #Testing euro dot combination. The € character right dot combination is 56-15.
    {
        'txt':  'A vételár 1 €.', 
        'brl':  '$a v1tel"r #a <e.', 
        'BRLCursorPos': 1,
    },
    #Testing left and right parentheses dot combinations. Left parenthese dot combination is 2346, right parenthese dot combination is 1356.
    {
        'txt':  'De nekem nagyon nehezemre esik a levélírás (nézd el a helyesírási hibákat és a csúnya írásomat).', 
        'brl':  '$de nekem na4on nehe2emre esik a lev1l|r"s ~n12d el a hees|r"si hib"kat 1s a 306a |r"somatz.', 
        'BRLCursorPos': 1,
    },
    #Testing capsign indicator. If a word entire containing uppercase letter, need marking this with two 46 dot combination before the word.
    {
        'txt':  'A kiállítás megnyitóján jelen volt az MVGYOSZ elnöke.', 
        'brl':  '$a ki"ll|t"s meg6it9j"n jelen volt a2 $$mv4o5 elnqke.', 
        'BRLCursorPos': 1,
    },
    #Testing time format. If a sentence containing 11:45 style text part, need replacing the colon 25 dot combination with dot3 combination, and not need marking numbersign indicator the next numbers.
    {
        'txt':  'A pontos idő 11:45 perc.', 
        'brl':  '$a pontos id7 #aa.de perc.', 
        'BRLCursorPos': 1,
    },
    #Testing bulleted list item, the right dot combination is 3-35.
    {
        'txt':  '•A kiadás újdonságai.', 
        'brl':  """'*$a kiad"s 0jdons"gai.""", 
    },
    #Testing underscore, the right dot combination is 6-36.
    {
        'txt':  'A hu_list fájl nem létezik.', 
        'brl':  """$a hu'-list f"jl nem l1te2ik.""", 
        'BRLCursorPos': 1,
    },
    #Testing a sentance containing an e-mail address, the dot combination for at sign is dots 45
    {
        'txt':  'Az e-mail címem: akarmi@akarmi.hu.', 
        'brl':  '$a2 e-mail c|mem: akarmi>akarmi.hu.', 
        'BRLCursorPos': 1,
    },
]
