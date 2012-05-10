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
        'txt':  u'adásszerű', 
        'brl':  u'ad"s5er}', 
    }, {
        'txt':  u'adásszünet', 
        'brl':  u'ad"s5{net', 
    }, {
        'txt':  u'adósszámla', 
        'brl':  u'ad9s5"mla', 
    }, {
        'txt':  u'Agyagosszergény', 
        'brl':  u'$a4agos5erg16', 
    }, {
        'txt':  u'agyaggyűrűiken', 
        'brl':  u'a4ag4}r}iken', 
    }, {
        'txt':  u'agyonnyom', 
        'brl':  u'a4on6om', 
    }, {
        'txt':  u'agyonnyúzott', 
        'brl':  u'a4on602ott', 
    }, {
        'txt':  u'agyonnyűtt ', 
        'brl':  u'a4on6}tt ', 
    }, {
        'txt':  u'ágyússzekerek', 
        'brl':  u'"40s5ekerek', 
    }, {
        'txt':  u'ájulásszerű', 
        'brl':  u'"jul"s5er}', 
    }, {
        'txt':  u'akácsövény', 
        'brl':  u'ak"csqv16', 
    }, {
        'txt':  u'ákácsövény', 
        'brl':  u'"k"csqv16', 
    }, {
        'txt':  u'alásszolgája', 
        'brl':  u'al"s5olg"ja', 
    }, {
        'txt':  u'alásszolgájuk ', 
        'brl':  u'al"s5olg"juk ', 
    }, {
        'txt':  u'alkalmazásszerver', 
        'brl':  u'alkalma2"s5erver', 
    }, {
        'txt':  u'állásszög', 
        'brl':  u'"ll"s5qg', 
    }, {
        'txt':  u'almásszürke', 
        'brl':  u'alm"s5{rke', 
    }, {
        'txt':  u'alvásszegény', 
        'brl':  u'alv"s5eg16', 
    }, {
        'txt':  u'alvásszükséglete', 
        'brl':  u'alv"s5{ks1glete', 
    }, {
        'txt':  u'alvászavar', 
        'brl':  u'alv"s2avar', 
    }, {
        'txt':  u'anyaggyőző', 
        'brl':  u'a6ag4727', 
    }, {
        'txt':  u'anyaggyűjtés', 
        'brl':  u'a6ag4}jt1s', 
    }, {
        'txt':  u'aranyosszőke', 
        'brl':  u'ara6os57ke', 
    }, {
        'txt':  u'árboccsúcs', 
        'brl':  u'"rboc303', 
    }, {
        'txt':  u'árbóccsúcs', 
        'brl':  u'"rb9c303', 
    }, {
        'txt':  u'árbocsudarat', 
        'brl':  u'"rbocsudarat', 
    }, {
        'txt':  u'arcüreggyulladás', 
        'brl':  u'arc{reg4ullad"s', 
    }, {
        'txt':  u'arccsont', 
        'brl':  u'arc3ont', 
    }, {
        'txt':  u'arcseb', 
        'brl':  u'arcseb', 
    }, {
        'txt':  u'arcsebe ', 
        'brl':  u'arcsebe ', 
    }, {
        'txt':  u'arcsérülés', 
        'brl':  u'arcs1r{l1s', 
    }, {
        'txt':  u'árgusszemű', 
        'brl':  u'"rgus5em}', 
    }, {
        'txt':  u'árvízsújtotta', 
        'brl':  u'"rv|2s0jtotta', 
    }, {
        'txt':  u'autósszemüveg', 
        'brl':  u'aut9s5em{veg', 
    }, {
        'txt':  u'azonnyomban', 
        'brl':  u'a2on6omban', 
    }, {
        'txt':  u'bádoggyűjtemény', 
        'brl':  u'b"dog4}jtem16', 
    }, {
        'txt':  u'barnásszőke', 
        'brl':  u'barn"s57ke', 
    }, {
        'txt':  u'barnásszőkét', 
        'brl':  u'barn"s57k1t', 
    }, {
        'txt':  u'barnásszürke', 
        'brl':  u'barn"s5{rke', 
    }, {
        'txt':  u'barnesszal', 
        'brl':  u'barne55al', 
    }, {
        'txt':  u'becslésszerűen', 
        'brl':  u'be3l1s5er}en', 
    }, {
        'txt':  u'Békésszentandrás', 
        'brl':  u'$b1k1s5entandr"s', 
    }, {
        'txt':  u'bélésszövet', 
        'brl':  u'b1l1s5qvet', 
    }, {
        'txt':  u'bélyeggyűjtemény', 
        'brl':  u'b1eg4}jtem16', 
    }, {
        'txt':  u'bércsüveg', 
        'brl':  u'b1rcs{veg', 
    }, {
        'txt':  u'berendezésszerű ', 
        'brl':  u'berende21s5er} ', 
    }, {
        'txt':  u'berendezésszett', 
        'brl':  u'berende21s5ett', 
    }, {
        'txt':  u'beteggyógyász', 
        'brl':  u'beteg494"5', 
    }, {
        'txt':  u'bilincszörgés', 
        'brl':  u'bilin32qrg1s', 
    }, {
        'txt':  u'binsenggyökér', 
        'brl':  u'binsen44qk1r', 
    }, {
        'txt':  u'bohócsapkája', 
        'brl':  u'boh9csapk"ja', 
    }, {
        'txt':  u'bonbonmeggy', 
        'brl':  u'bonbonme44', 
    }, {
        'txt':  u'borsszem', 
        'brl':  u'bors5em', 
    }, {
        'txt':  u'borsszóró', 
        'brl':  u'bors59r9', 
    }, {
        'txt':  u'borzasszőrű', 
        'brl':  u'bor2as57r}', 
    }, {
        'txt':  u'borzzsír', 
        'brl':  u'bor2`|r', 
    }, {
        'txt':  u'bőgésszerű', 
        'brl':  u'b7g1s5er}', 
    }, {
        'txt':  u'börtönnyelve', 
        'brl':  u'bqrtqn6elve', 
    }, {
        'txt':  u'brekegésszerű', 
        'brl':  u'brekeg1s5er}', 
    }, {
        'txt':  u'bronzsáska', 
        'brl':  u'bron2s"ska', 
    }, {
        'txt':  u'bronzsáskák', 
        'brl':  u'bron2s"sk"k', 
    }, {
        'txt':  u'bronzsasokkal ', 
        'brl':  u'bron2sasokkal ', 
    }, {
        'txt':  u'bronzsisak', 
        'brl':  u'bron2sisak', 
    }, {
        'txt':  u'búcsújárásszerű', 
        'brl':  u'b030j"r"s5er}', 
    }, {
        'txt':  u'bűnnyomok', 
        'brl':  u'b}n6omok', 
    }, {
        'txt':  u'chipseszacskó', 
        'brl':  u'chipses2a3k9', 
    }, {
        'txt':  u'csapásszám', 
        'brl':  u'3ap"s5"m', 
    }, {
        'txt':  u'csárdásszóló', 
        'brl':  u'3"rd"s59l9', 
    }, {
        'txt':  u'csattanásszerű', 
        'brl':  u'3attan"s5er}', 
    }, {
        'txt':  u'csavarásszerű', 
        'brl':  u'3avar"s5er}', 
    }, {
        'txt':  u'csikósszámadó', 
        'brl':  u'3ik9s5"mad9', 
    }, {
        'txt':  u'csipkésszélű', 
        'brl':  u'3ipk1s51l}', 
    }, {
        'txt':  u'csobbanásszerű', 
        'brl':  u'3obban"s5er}', 
    }, {
        'txt':  u'csuklásszerű', 
        'brl':  u'3ukl"s5er}', 
    }, {
        'txt':  u'disszertáció', 
        'brl':  u'di55ert"ci9', 
    }, {
        'txt':  u'dobpergésszerűen', 
        'brl':  u'dobperg1s5er}en', 
    }, {
        'txt':  u'döggyapjú', 
        'brl':  u'dqg4apj0', 
    }, {
        'txt':  u'dőlésszög', 
        'brl':  u'd7l1s5qg', 
    }, {
        'txt':  u'dörgésszerű', 
        'brl':  u'dqrg1s5er}', 
    }, {
        'txt':  u'dörgésszerű ', 
        'brl':  u'dqrg1s5er} ', 
    }, {
        'txt':  u'dragonyosszázad ', 
        'brl':  u'drago6os5"2ad ', 
    }, {
        'txt':  u'dragonyoszászlóalj', 
        'brl':  u'drago6os2"5l9alj', 
    }, {
        'txt':  u'droggyanús', 
        'brl':  u'drog4an0s', 
    }, {
        'txt':  u'dússzakállú', 
        'brl':  u'd0s5ak"ll0', 
    }, {
        'txt':  u'édesszájú', 
        'brl':  u'1des5"j0', 
    }, {
        'txt':  u'édesszesztestvér', 
        'brl':  u'1des5e5testv1r', 
    }, {
        'txt':  u'égésszabály', 
        'brl':  u'1g1s5ab"', 
    }, {
        'txt':  u'égésszag', 
        'brl':  u'1g1s5ag', 
    }, {
        'txt':  u'égésszám', 
        'brl':  u'1g1s5"m', 
    }, {
        'txt':  u'égésszigetelés', 
        'brl':  u'1g1s5igetel1s', 
    }, {
        'txt':  u'egyenesszálú', 
        'brl':  u'e4enes5"l0', 
    }, {
        'txt':  u'egyenesszárnyúak', 
        'brl':  u'e4enes5"r60ak', 
    }, {
        'txt':  u'egyenesszög', 
        'brl':  u'e4enes5qg', 
    }, {
        'txt':  u'egyezség', 
        'brl':  u'e4e2s1g', 
    }, {
        'txt':  u'éhesszájat ', 
        'brl':  u'1hes5"jat ', 
    }, {
        'txt':  u'ejtőernyősszárnyak', 
        'brl':  u'ejt7er67s5"r6ak', 
    }, {
        'txt':  u'ejtőernyősszázad', 
        'brl':  u'ejt7er67s5"2ad', 
    }, {
        'txt':  u'ejtőernyőszászlóalj ', 
        'brl':  u'ejt7er67s2"5l9alj ', 
    }, {
        'txt':  u'ékesszólás', 
        'brl':  u'1kes59l"s', 
    }, {
        'txt':  u'ékesszóló ', 
        'brl':  u'1kes59l9 ', 
    }, {
        'txt':  u'ekhósszekér', 
        'brl':  u'ekh9s5ek1r', 
    }, {
        'txt':  u'ekhósszekerek', 
        'brl':  u'ekh9s5ekerek', 
    }, {
        'txt':  u'eleséggyűjtés ', 
        'brl':  u'eles1g4}jt1s ', 
    }, {
        'txt':  u'élesszemű', 
        'brl':  u'1les5em}', 
    }, {
        'txt':  u'ellátásszerű', 
        'brl':  u'ell"t"s5er}', 
    }, {
        'txt':  u'ellenállásszekrény', 
        'brl':  u'ellen"ll"s5ekr16', 
    }, {
        'txt':  u'ellennyilatkozat', 
        'brl':  u'ellen6ilatko2at', 
    }, {
        'txt':  u'ellennyomás', 
        'brl':  u'ellen6om"s', 
    }, {
        'txt':  u'elméncség', 
        'brl':  u'elm1ncs1g', 
    }, {
        'txt':  u'előírásszerű ', 
        'brl':  u'el7|r"s5er} ', 
    }, {
        'txt':  u'elrémisszék ', 
        'brl':  u'elr1mi551k ', 
    }, {
        'txt':  u'emberhússzagot ', 
        'brl':  u'emberh0s5agot ', 
    }, {
        'txt':  u'emelésszerű', 
        'brl':  u'emel1s5er}', 
    }, {
        'txt':  u'érccsapadék, érccsengés, érccsatorna ', 
        'brl':  u'1rc3apad1k, 1rc3eng1s, 1rc3atorna ', 
    }, {
        'txt':  u'ércsalak', 
        'brl':  u'1rcsalak', 
    }, {
        'txt':  u'ércsas', 
        'brl':  u'1rcsas', 
    }, {
        'txt':  u'ércselyem', 
        'brl':  u'1rcseem', 
    }, {
        'txt':  u'ércsíp, ércsípjába, ércsípláda ', 
        'brl':  u'1rcs|p, 1rcs|pj"ba, 1rcs|pl"da ', 
    }, {
        'txt':  u'ércsíptető', 
        'brl':  u'1r3|ptet7', 
    }, {
        'txt':  u'ércsisak', 
        'brl':  u'1rcsisak', 
    }, {
        'txt':  u'ércsodrony', 
        'brl':  u'1rcsodro6', 
    }, {
        'txt':  u'erőforrászabáló', 
        'brl':  u'er7forr"s2ab"l9', 
    }, {
        'txt':  u'érzékelésszint', 
        'brl':  u'1r21kel1s5int', 
    }, {
        'txt':  u'ésszerű', 
        'brl':  u'155er}', 
    }, {
        'txt':  u'eszközsor, eszközsorán', 
        'brl':  u'e5kq2sor, e5kq2sor"n', 
    }, {
        'txt':  u'evészavar', 
        'brl':  u'ev1s2avar', 
    }, {
        'txt':  u'fagyosszentek', 
        'brl':  u'fa4os5entek', 
    }, {
        'txt':  u'fáklyászene', 
        'brl':  u'f"k"s2ene', 
    }, {
        'txt':  u'farkasszáj ', 
        'brl':  u'farkas5"j ', 
    }, {
        'txt':  u'farkasszem', 
        'brl':  u'farkas5em', 
    }, {
        'txt':  u'farkasszemet ', 
        'brl':  u'farkas5emet ', 
    }, {
        'txt':  u'Farkassziget,', 
        'brl':  u'$farkas5iget,', 
    }, {
        'txt':  u'fásszárú', 
        'brl':  u'f"s5"r0', 
    }, {
        'txt':  u'fegyenccsoport', 
        'brl':  u'fe4enc3oport', 
    }, {
        'txt':  u'fegyencsapkát ', 
        'brl':  u'fe4encsapk"t ', 
    }, {
        'txt':  u'fehéresszőke, fehéresszürke ', 
        'brl':  u'feh1res57ke, feh1res5{rke ', 
    }, {
        'txt':  u'feketésszürke', 
        'brl':  u'feket1s5{rke', 
    }, {
        'txt':  u'feleséggyilkos', 
        'brl':  u'feles1g4ilkos', 
    }, {
        'txt':  u'felfedezésszámba', 
        'brl':  u'felfede21s5"mba', 
    }, {
        'txt':  u'felséggyilkolás', 
        'brl':  u'fels1g4ilkol"s', 
    }, {
        'txt':  u'felszerelésszettet', 
        'brl':  u'fel5erel1s5ettet', 
    }, {
        'txt':  u'fertőzésszerű', 
        'brl':  u'fert721s5er}', 
    }, {
        'txt':  u'filccsizma', 
        'brl':  u'filc3i2ma', 
    }, {
        'txt':  u'filigránnyelű', 
        'brl':  u'filigr"n6el}', 
    }, {
        'txt':  u'fogasszeg', 
        'brl':  u'fogas5eg', 
    }, {
        'txt':  u'fogfájásszerű', 
        'brl':  u'fogf"j"s5er}', 
    }, {
        'txt':  u'foglalkozásszerű', 
        'brl':  u'foglalko2"s5er}', 
    }, {
        'txt':  u'foggyalu', 
        'brl':  u'fog4alu', 
    }, {
        'txt':  u'foggyökér', 
        'brl':  u'fog4qk1r', 
    }, {
        'txt':  u'foggyulladás', 
        'brl':  u'fog4ullad"s', 
    }, {
        'txt':  u'foggyűrű', 
        'brl':  u'fog4}r}', 
    }, {
        'txt':  u'forgásszabály', 
        'brl':  u'forg"s5ab"', 
    }, {
        'txt':  u'forrásszöveg', 
        'brl':  u'forr"s5qveg', 
    }, {
        'txt':  u'fosszínű', 
        'brl':  u'fos5|n}', 
    }, {
        'txt':  u'földcsuszamlásszerűen', 
        'brl':  u'fqld3u5aml"s5er}en', 
    }, {
        'txt':  u'fölélesszem', 
        'brl':  u'fql1le55em', 
    }, {
        'txt':  u'főzőkalánnyelet', 
        'brl':  u'f727kal"n6elet', 
    }, {
        'txt':  u'fuvarosszekér', 
        'brl':  u'fuvaros5ek1r', 
    }, {
        'txt':  u'fúvósszerszám', 
        'brl':  u'f0v9s5er5"m', 
    }, {
        'txt':  u'fúvósszimfónia', 
        'brl':  u'f0v9s5imf9nia', 
    }, {
        'txt':  u'fúvószenekar', 
        'brl':  u'f0v9s2enekar', 
    }, {
        'txt':  u'fűtésszag', 
        'brl':  u'f}t1s5ag', 
    }, {
        'txt':  u'garaboncsereg', 
        'brl':  u'garaboncsereg', 
    }, {
        'txt':  u'gázspray', 
        'brl':  u'g"2spray', 
    }, {
        'txt':  u'gázsugár', 
        'brl':  u'g"2sug"r', 
    }, {
        'txt':  u'gerincsérült', 
        'brl':  u'gerincs1r{lt', 
    }, {
        'txt':  u'gerincsérv ', 
        'brl':  u'gerincs1rv ', 
    }, {
        'txt':  u'ginszenggyökér', 
        'brl':  u'gin5eng4qk1r', 
    }, {
        'txt':  u'ginzenggyökér', 
        'brl':  u'gin2eng4qk1r', 
    }, {
        'txt':  u'Gombosszeg', 
        'brl':  u'$gombos5eg', 
    }, {
        'txt':  u'gondviselésszerű', 
        'brl':  u'gondvisel1s5er}', 
    }, {
        'txt':  u'gőzsíp', 
        'brl':  u'g72s|p', 
    }, {
        'txt':  u'gőzsugár', 
        'brl':  u'g72sug"r', 
    }, {
        'txt':  u'gőzszivattyú', 
        'brl':  u'g725iva880', 
    }, {
        'txt':  u'gránátoszászlóalj', 
        'brl':  u'gr"n"tos2"5l9alj', 
    }, {
        'txt':  u'gúnyversszerző', 
        'brl':  u'g06vers5er27', 
    }, {
        'txt':  u'gyalogosszázad', 
        'brl':  u'4alogos5"2ad', 
    }, {
        'txt':  u'gyalogoszászlóalj,', 
        'brl':  u'4alogos2"5l9alj,', 
    }, {
        'txt':  u'gyorsszárnyú', 
        'brl':  u'4ors5"r60', 
    }, {
        'txt':  u'gyorsszekér ', 
        'brl':  u'4ors5ek1r ', 
    }, {
        'txt':  u'gyorsszűrő', 
        'brl':  u'4ors5}r7', 
    }, {
        'txt':  u'gyújtásszabály', 
        'brl':  u'40jt"s5ab"', 
    }, {
        'txt':  u'gyújtászsinór', 
        'brl':  u'40jt"s`in9r', 
    }, {
        'txt':  u'gyűlésszíne', 
        'brl':  u'4}l1s5|ne', 
    }, {
        'txt':  u'habarccsal', 
        'brl':  u'habar33al', 
    }, {
        'txt':  u'habitusszerűen', 
        'brl':  u'habitus5er}en', 
    }, {
        'txt':  u'hadianyaggyár', 
        'brl':  u'hadia6ag4"r', 
    }, {
        'txt':  u'hadsereggyűjtés ', 
        'brl':  u'hadsereg4}jt1s ', 
    }, {
        'txt':  u'hajlásszög', 
        'brl':  u'hajl"s5qg', 
    }, {
        'txt':  u'hajósszekerce ', 
        'brl':  u'haj9s5ekerce ', 
    }, {
        'txt':  u'hajósszemélyzet', 
        'brl':  u'haj9s5em12et', 
    }, {
        'txt':  u'hallászavar', 
        'brl':  u'hall"s2avar', 
    }, {
        'txt':  u'halottasszekér ', 
        'brl':  u'halottas5ek1r ', 
    }, {
        'txt':  u'halottasszoba', 
        'brl':  u'halottas5oba', 
    }, {
        'txt':  u'halottasszobába', 
        'brl':  u'halottas5ob"ba', 
    }, {
        'txt':  u'hamvasszőke', 
        'brl':  u'hamvas57ke', 
    }, {
        'txt':  u'hamvasszürke', 
        'brl':  u'hamvas5{rke', 
    }, {
        'txt':  u'hanggyakorlat', 
        'brl':  u'hang4akorlat', 
    }, {
        'txt':  u'hányásszag', 
        'brl':  u'h"6"s5ag', 
    }, {
        'txt':  u'haragoszöld', 
        'brl':  u'haragos2qld', 
    }, {
        'txt':  u'harcosszellem', 
        'brl':  u'harcos5ellem', 
    }, {
        'txt':  u'harccselekmény ', 
        'brl':  u'harc3elekm16 ', 
    }, {
        'txt':  u'harccsoport', 
        'brl':  u'harc3oport', 
    }, {
        'txt':  u'harcsor', 
        'brl':  u'harcsor', 
    }, {
        'txt':  u'hármasszámú', 
        'brl':  u'h"rmas5"m0', 
    }, {
        'txt':  u'hársszén', 
        'brl':  u'h"rs51n', 
    }, {
        'txt':  u'hársszenet', 
        'brl':  u'h"rs5enet', 
    }, {
        'txt':  u'hártyásszárnyú', 
        'brl':  u'h"r8"s5"r60', 
    }, {
        'txt':  u'hasisszagot', 
        'brl':  u'hasis5agot', 
    }, {
        'txt':  u'hatásszünet', 
        'brl':  u'hat"s5{net', 
    }, {
        'txt':  u'házsárkodását', 
        'brl':  u'h"`"rkod"s"t', 
    }, {
        'txt':  u'hegyesszög', 
        'brl':  u'he4es5qg', 
    }, {
        'txt':  u'hegyszorosszerű', 
        'brl':  u'he45oros5er}', 
    }, {
        'txt':  u'hekusszagot', 
        'brl':  u'hekus5agot', 
    }, {
        'txt':  u'hentesszaktanfolyamát', 
        'brl':  u'hentes5aktanfoam"t', 
    }, {
        'txt':  u'hirdetésszöveg', 
        'brl':  u'hirdet1s5qveg', 
    }, {
        'txt':  u'hivatásszerűen', 
        'brl':  u'hivat"s5er}en', 
    }, {
        'txt':  u'hízelkedésszámba', 
        'brl':  u'h|2elked1s5"mba', 
    }, {
        'txt':  u'hólyaggyulladás', 
        'brl':  u'h9ag4ullad"s', 
    }, {
        'txt':  u'hörgésszerű', 
        'brl':  u'hqrg1s5er}', 
    }, {
        'txt':  u'hősszínész', 
        'brl':  u'h7s5|n15', 
    }, {
        'txt':  u'hősszövetség ', 
        'brl':  u'h7s5qvets1g ', 
    }, {
        'txt':  u'hússzaft', 
        'brl':  u'h0s5aft', 
    }, {
        'txt':  u'hússzag', 
        'brl':  u'h0s5ag', 
    }, {
        'txt':  u'hússzagú', 
        'brl':  u'h0s5ag0', 
    }, {
        'txt':  u'hússzállítmány ', 
        'brl':  u'h0s5"ll|tm"6 ', 
    }, {
        'txt':  u'hússzállító', 
        'brl':  u'h0s5"ll|t9', 
    }, {
        'txt':  u'hússzalonna', 
        'brl':  u'h0s5alonna', 
    }, {
        'txt':  u'hússzekrény', 
        'brl':  u'h0s5ekr16', 
    }, {
        'txt':  u'hússzelet', 
        'brl':  u'h0s5elet', 
    }, {
        'txt':  u'Hússziget', 
        'brl':  u'$h0s5iget', 
    }, {
        'txt':  u'hússzínű', 
        'brl':  u'h0s5|n}', 
    }, {
        'txt':  u'hűvösszemű', 
        'brl':  u'h}vqs5em}', 
    }, {
        'txt':  u'ideggyengeség', 
        'brl':  u'ideg4enges1g', 
    }, {
        'txt':  u'ideggyógyászat', 
        'brl':  u'ideg494"5at', 
    }, {
        'txt':  u'ideggyógyintézet', 
        'brl':  u'ideg494int12et', 
    }, {
        'txt':  u'ideggyönge', 
        'brl':  u'ideg4qnge', 
    }, {
        'txt':  u'ideggyötrő', 
        'brl':  u'ideg4qtr7', 
    }, {
        'txt':  u'ideggyulladás', 
        'brl':  u'ideg4ullad"s', 
    }, {
        'txt':  u'identitászavar', 
        'brl':  u'identit"s2avar', 
    }, {
        'txt':  u'időjárásszolgálat', 
        'brl':  u'id7j"r"s5olg"lat', 
    }, {
        'txt':  u'imádsággyűjtemény', 
        'brl':  u'im"ds"g4}jtem16', 
    }, {
        'txt':  u'inasszerep', 
        'brl':  u'inas5erep', 
    }, {
        'txt':  u'inasszerepet ', 
        'brl':  u'inas5erepet ', 
    }, {
        'txt':  u'inasszeretetet', 
        'brl':  u'inas5eretetet', 
    }, {
        'txt':  u'indiánnyelv', 
        'brl':  u'indi"n6elv', 
    }, {
        'txt':  u'ínnyújtó,', 
        'brl':  u'|n60jt9,', 
    }, {
        'txt':  u'ínnyújtót ', 
        'brl':  u'|n60jt9t ', 
    }, {
        'txt':  u'ínyencség', 
        'brl':  u'|6encs1g', 
    }, {
        'txt':  u'írásszeretet,', 
        'brl':  u'|r"s5eretet,', 
    }, {
        'txt':  u'irtásszél', 
        'brl':  u'irt"s51l', 
    }, {
        'txt':  u'istennyila', 
        'brl':  u'isten6ila', 
    }, {
        'txt':  u'járásszerű ', 
        'brl':  u'j"r"s5er} ', 
    }, {
        'txt':  u'jáspisszobor', 
        'brl':  u'j"spis5obor', 
    }, {
        'txt':  u'jegeccsoport', 
        'brl':  u'jegec3oport', 
    }, {
        'txt':  u'jéggyártás', 
        'brl':  u'j1g4"rt"s', 
    }, {
        'txt':  u'jelenésszerű ', 
        'brl':  u'jelen1s5er} ', 
    }, {
        'txt':  u'jelentésszerű', 
        'brl':  u'jelent1s5er}', 
    }, {
        'txt':  u'jelentésszint', 
        'brl':  u'jelent1s5int', 
    }, {
        'txt':  u'jelzésszerű', 
        'brl':  u'jel21s5er}', 
    }, {
        'txt':  u'jósszavai,', 
        'brl':  u'j9s5avai,', 
    }, {
        'txt':  u'jósszelleme,', 
        'brl':  u'j9s5elleme,', 
    }, {
        'txt':  u'kabinnyíláson', 
        'brl':  u'kabin6|l"son', 
    }, {
        'txt':  u'kabinnyomás', 
        'brl':  u'kabin6om"s', 
    }, {
        'txt':  u'kakasszó', 
        'brl':  u'kakas59', 
    }, {
        'txt':  u'kalapácszengés', 
        'brl':  u'kalap"32eng1s', 
    }, {
        'txt':  u'kamionnyi', 
        'brl':  u'kamion6i', 
    }, {
        'txt':  u'kaparásszerű', 
        'brl':  u'kapar"s5er}', 
    }, {
        'txt':  u'Kaposszekcső', 
        'brl':  u'$kapos5ek37', 
    }, {
        'txt':  u'Kaposszerdahely', 
        'brl':  u'$kapos5erdahe', 
    }, {
        'txt':  u'kapusszoba', 
        'brl':  u'kapus5oba', 
    }, {
        'txt':  u'karosszék', 
        'brl':  u'karos51k', 
    }, {
        'txt':  u'kartácszápor', 
        'brl':  u'kart"32"por', 
    }, {
        'txt':  u'kartonnyi', 
        'brl':  u'karton6i', 
    }, {
        'txt':  u'kasszék', 
        'brl':  u'kas51k', 
    }, {
        'txt':  u'katalógusszám', 
        'brl':  u'katal9gus5"m', 
    }, {
        'txt':  u'katekizmusszerű', 
        'brl':  u'kateki2mus5er}', 
    }, {
        'txt':  u'kavarásszerű', 
        'brl':  u'kavar"s5er}', 
    }, {
        'txt':  u'kavicsszerű ', 
        'brl':  u'kavi35er} ', 
    }, {
        'txt':  u'kavicszápor', 
        'brl':  u'kavi32"por', 
    }, {
        'txt':  u'kavicszátony', 
        'brl':  u'kavi32"to6', 
    }, {
        'txt':  u'kavicszuzalék', 
        'brl':  u'kavi32u2al1k', 
    }, {
        'txt':  u'kékesszürke', 
        'brl':  u'k1kes5{rke', 
    }, {
        'txt':  u'Kemenesszentmárton', 
        'brl':  u'$kemenes5entm"rton', 
    }, {
        'txt':  u'Kemenesszentpéter', 
        'brl':  u'$kemenes5entp1ter', 
    }, {
        'txt':  u'képmásszerű ', 
        'brl':  u'k1pm"s5er} ', 
    }, {
        'txt':  u'képzésszerű ', 
        'brl':  u'k1p21s5er} ', 
    }, {
        'txt':  u'képzésszervezés', 
        'brl':  u'k1p21s5erve21s', 
    }, {
        'txt':  u'kerekesszék', 
        'brl':  u'kerekes51k', 
    }, {
        'txt':  u'keresésszolgáltató', 
        'brl':  u'keres1s5olg"ltat9', 
    }, {
        'txt':  u'kérésszerűen', 
        'brl':  u'k1r1s5er}en', 
    }, {
        'txt':  u'kerítésszaggató', 
        'brl':  u'ker|t1s5aggat9', 
    }, {
        'txt':  u'késszúrás', 
        'brl':  u'k1s50r"s', 
    }, {
        'txt':  u'kevésszavú ', 
        'brl':  u'kev1s5av0 ', 
    }, {
        'txt':  u'kevésszer', 
        'brl':  u'kev1s5er', 
    }, {
        'txt':  u'kifejlesszem', 
        'brl':  u'kifejle55em', 
    }, {
        'txt':  u'kihívásszerű', 
        'brl':  u'kih|v"s5er}', 
    }, {
        'txt':  u'kilenccsatorna', 
        'brl':  u'kilenc3atorna', 
    }, {
        'txt':  u'kilincszörgést ', 
        'brl':  u'kilin32qrg1st ', 
    }, {
        'txt':  u'Kisszállás', 
        'brl':  u'$kis5"ll"s', 
    }, {
        'txt':  u'kisszámú', 
        'brl':  u'kis5"m0', 
    }, {
        'txt':  u'Kisszeben', 
        'brl':  u'$kis5eben', 
    }, {
        'txt':  u'kisszék', 
        'brl':  u'kis51k', 
    }, {
        'txt':  u'kisszekrény', 
        'brl':  u'kis5ekr16', 
    }, {
        'txt':  u'Kisszentmárton', 
        'brl':  u'$kis5entm"rton', 
    }, {
        'txt':  u'kisszerű', 
        'brl':  u'kis5er}', 
    }, {
        'txt':  u'Kissziget', 
        'brl':  u'$kis5iget', 
    }, {
        'txt':  u'kisszobában', 
        'brl':  u'kis5ob"ban', 
    }, {
        'txt':  u'kisszótár', 
        'brl':  u'kis59t"r', 
    }, {
        'txt':  u'Kiszombor', 
        'brl':  u'$kis2ombor', 
    }, {
        'txt':  u'kiszögellésszerűen', 
        'brl':  u'ki5qgell1s5er}en', 
    }, {
        'txt':  u'Kiszsidány', 
        'brl':  u'$kis`id"6', 
    }, {
        'txt':  u'kitörésszerű', 
        'brl':  u'kitqr1s5er}', 
    }, {
        'txt':  u'kitüntetésszalagokat', 
        'brl':  u'kit{ntet1s5alagokat', 
    }, {
        'txt':  u'kliensszoftver', 
        'brl':  u'kliens5oftver', 
    }, {
        'txt':  u'kóccsomó', 
        'brl':  u'k9c3om9', 
    }, {
        'txt':  u'koldusszáj', 
        'brl':  u'koldus5"j', 
    }, {
        'txt':  u'koldusszakáll', 
        'brl':  u'koldus5ak"ll', 
    }, {
        'txt':  u'koldusszegény', 
        'brl':  u'koldus5eg16', 
    }, {
        'txt':  u'kolduszene ', 
        'brl':  u'koldus2ene ', 
    }, {
        'txt':  u'kommunikációsszoba', 
        'brl':  u'kommunik"ci9s5oba', 
    }, {
        'txt':  u'komposszesszor', 
        'brl':  u'kompo55e55or', 
    }, {
        'txt':  u'komposszesszorátus ', 
        'brl':  u'kompo55e55or"tus ', 
    }, {
        'txt':  u'kormosszürke', 
        'brl':  u'kormos5{rke', 
    }, {
        'txt':  u'kosszarv', 
        'brl':  u'kos5arv', 
    }, {
        'txt':  u'kosszem', 
        'brl':  u'kos5em', 
    }, {
        'txt':  u'köhögésszerű', 
        'brl':  u'kqhqg1s5er}', 
    }, {
        'txt':  u'kőművesszem', 
        'brl':  u'k7m}ves5em', 
    }, {
        'txt':  u'kőművesszerszámait ', 
        'brl':  u'k7m}ves5er5"mait ', 
    }, {
        'txt':  u'köntösszegély', 
        'brl':  u'kqntqs5eg1', 
    }, {
        'txt':  u'könyöklésszéles', 
        'brl':  u'kq6qkl1s51les', 
    }, {
        'txt':  u'könyvesszekrény', 
        'brl':  u'kq6ves5ekr16', 
    }, {
        'txt':  u'kőrisszár', 
        'brl':  u'k7ris5"r', 
    }, {
        'txt':  u'kőrisszárat ', 
        'brl':  u'k7ris5"rat ', 
    }, {
        'txt':  u'körösszakál', 
        'brl':  u'kqrqs5ak"l', 
    }, {
        'txt':  u'körösszakáli ', 
        'brl':  u'kqrqs5ak"li ', 
    }, {
        'txt':  u'Köröszug', 
        'brl':  u'$kqrqs2ug', 
    }, {
        'txt':  u'kötelességgyakorlás', 
        'brl':  u'kqteless1g4akorl"s', 
    }, {
        'txt':  u'közsáv', 
        'brl':  u'kq2s"v', 
    }, {
        'txt':  u'közsereg', 
        'brl':  u'kq2sereg', 
    }, {
        'txt':  u'kudarcsorozat', 
        'brl':  u'kudarcsoro2at', 
    }, {
        'txt':  u'kulcszörgés', 
        'brl':  u'kul32qrg1s', 
    }, {
        'txt':  u'kuplunggyár', 
        'brl':  u'kuplung4"r', 
    }, {
        'txt':  u'kurucság', 
        'brl':  u'kurucs"g', 
    }, {
        'txt':  u'küldetésszaga', 
        'brl':  u'k{ldet1s5aga', 
    }, {
        'txt':  u'különcség', 
        'brl':  u'k{lqncs1g', 
    }, {
        'txt':  u'labirintusszerű', 
        'brl':  u'labirintus5er}', 
    }, {
        'txt':  u'lakásszentelő', 
        'brl':  u'lak"s5entel7', 
    }, {
        'txt':  u'lakásszövetkezet', 
        'brl':  u'lak"s5qvetke2et', 
    }, {
        'txt':  u'lakosszám', 
        'brl':  u'lakos5"m', 
    }, {
        'txt':  u'lánccsörgés ', 
        'brl':  u'l"nc3qrg1s ', 
    }, {
        'txt':  u'láncszem', 
        'brl':  u'l"nc5em', 
    }, {
        'txt':  u'lánggyújtogató', 
        'brl':  u'l"ng40jtogat9', 
    }, {
        'txt':  u'laposszárú', 
        'brl':  u'lapos5"r0', 
    }, {
        'txt':  u'látásszög', 
        'brl':  u'l"t"s5qg', 
    }, {
        'txt':  u'látászavar', 
        'brl':  u'l"t"s2avar', 
    }, {
        'txt':  u'látomásszerű', 
        'brl':  u'l"tom"s5er}', 
    }, {
        'txt':  u'lazaccsontváz', 
        'brl':  u'la2ac3ontv"2', 
    }, {
        'txt':  u'lázsebességgel', 
        'brl':  u'l"2sebess1ggel', 
    }, {
        'txt':  u'lázsóhajtás', 
        'brl':  u'l"2s9hajt"s', 
    }, {
        'txt':  u'léggyök', 
        'brl':  u'l1g4qk', 
    }, {
        'txt':  u'léggyökér ', 
        'brl':  u'l1g4qk1r ', 
    }, {
        'txt':  u'lejtésszög', 
        'brl':  u'lejt1s5qg', 
    }, {
        'txt':  u'lejtésszöge', 
        'brl':  u'lejt1s5qge', 
    }, {
        'txt':  u'lemezstúdió', 
        'brl':  u'leme2st0di9', 
    }, {
        'txt':  u'lengésszabály', 
        'brl':  u'leng1s5ab"', 
    }, {
        'txt':  u'lépcsősszárnyú', 
        'brl':  u'l1p37s5"r60', 
    }, {
        'txt':  u'lépésszám', 
        'brl':  u'l1p1s5"m', 
    }, {
        'txt':  u'lépészaj ', 
        'brl':  u'l1p1s2aj ', 
    }, {
        'txt':  u'léptennyomon', 
        'brl':  u'l1pten6omon', 
    }, {
        'txt':  u'levesszedő', 
        'brl':  u'leves5ed7', 
    }, {
        'txt':  u'liberty', 
        'brl':  u'liberty', 
    }, {
        'txt':  u'licenccsalád ', 
        'brl':  u'licenc3al"d ', 
    }, {
        'txt':  u'licencsértés', 
        'brl':  u'licencs1rt1s', 
    }, {
        'txt':  u'liszteszacskó', 
        'brl':  u'li5tes2a3k9', 
    }, {
        'txt':  u'loggyűjtemény', 
        'brl':  u'log4}jtem16', 
    }, {
        'txt':  u'lovasszázad', 
        'brl':  u'lovas5"2ad', 
    }, {
        'txt':  u'lovasszekeret ', 
        'brl':  u'lovas5ekeret ', 
    }, {
        'txt':  u'lökésszám', 
        'brl':  u'lqk1s5"m', 
    }, {
        'txt':  u'lökésszerű ', 
        'brl':  u'lqk1s5er} ', 
    }, {
        'txt':  u'lőporosszekér', 
        'brl':  u'l7poros5ek1r', 
    }, {
        'txt':  u'lőporosszekerek', 
        'brl':  u'l7poros5ekerek', 
    }, {
        'txt':  u'lőrésszerű', 
        'brl':  u'l7r1s5er}', 
    }, {
        'txt':  u'lugasszerű', 
        'brl':  u'lugas5er}', 
    }, {
        'txt':  u'luxusszálloda', 
        'brl':  u'luxus5"lloda', 
    }, {
        'txt':  u'macskakaparásszerű ', 
        'brl':  u'ma3kakapar"s5er} ', 
    }, {
        'txt':  u'magánnyelvmesterek', 
        'brl':  u'mag"n6elvmesterek', 
    }, {
        'txt':  u'magánnyugdíjpénztár', 
        'brl':  u'mag"n6ugd|jp1n2t"r', 
    }, {
        'txt':  u'magasszárú', 
        'brl':  u'magas5"r0', 
    }, {
        'txt':  u'mágnásszámba', 
        'brl':  u'm"gn"s5"mba', 
    }, {
        'txt':  u'mágnesszalag', 
        'brl':  u'm"gnes5alag', 
    }, {
        'txt':  u'mágnesszerű', 
        'brl':  u'm"gnes5er}', 
    }, {
        'txt':  u'mágneszár', 
        'brl':  u'm"gnes2"r', 
    }, {
        'txt':  u'malacság', 
        'brl':  u'malacs"g', 
    }, {
        'txt':  u'malacsült ', 
        'brl':  u'malacs{lt ', 
    }, {
        'txt':  u'málhásszekér', 
        'brl':  u'm"lh"s5ek1r', 
    }, {
        'txt':  u'málhásszekereiket ', 
        'brl':  u'm"lh"s5ekereiket ', 
    }, {
        'txt':  u'mandarinnyelv', 
        'brl':  u'mandarin6elv', 
    }, {
        'txt':  u'Marosszék', 
        'brl':  u'$maros51k', 
    }, {
        'txt':  u'Marosszentgyörgy ', 
        'brl':  u'$maros5ent4qr4 ', 
    }, {
        'txt':  u'Maroszug', 
        'brl':  u'$maros2ug', 
    }, {
        'txt':  u'Martraversszal', 
        'brl':  u'$martraver55al', 
    }, {
        'txt':  u'másszínű', 
        'brl':  u'm"s5|n}', 
    }, {
        'txt':  u'másszor', 
        'brl':  u'm"s5or', 
    }, {
        'txt':  u'másszóval', 
        'brl':  u'm"s59val', 
    }, {
        'txt':  u'másszőrűek', 
        'brl':  u'm"s57r}ek', 
    }, {
        'txt':  u'mécsesszem', 
        'brl':  u'm13es5em', 
    }, {
        'txt':  u'medresszék', 
        'brl':  u'medres51k', 
    }, {
        'txt':  u'meggybefőtt', 
        'brl':  u'me44bef7tt', 
    }, {
        'txt':  u'meggyel', 
        'brl':  u'me44el', 
    }, {
        'txt':  u'meggyen', 
        'brl':  u'me44en', 
    }, {
        'txt':  u'meggyes', 
        'brl':  u'me44es', 
    }, {
        'txt':  u'Meggyesi', 
        'brl':  u'$me44esi', 
    }, {
        'txt':  u'meggyet', 
        'brl':  u'me44et', 
    }, {
        'txt':  u'meggyfa', 
        'brl':  u'me44fa', 
    }, {
        'txt':  u'meggyhez', 
        'brl':  u'me44he2', 
    }, {
        'txt':  u'meggyízű', 
        'brl':  u'me44|2}', 
    }, {
        'txt':  u'meggylekvár', 
        'brl':  u'me44lekv"r', 
    }, {
        'txt':  u'meggymag', 
        'brl':  u'me44mag', 
    }, {
        'txt':  u'meggynek', 
        'brl':  u'me44nek', 
    }, {
        'txt':  u'meggypiros', 
        'brl':  u'me44piros', 
    }, {
        'txt':  u'meggyszín', 
        'brl':  u'me445|n', 
    }, {
        'txt':  u'meggytől', 
        'brl':  u'me44t7l', 
    }, {
        'txt':  u'meggytől', 
        'brl':  u'me44t7l', 
    }, {
        'txt':  u'meggyvörös', 
        'brl':  u'me44vqrqs', 
    }, {
        'txt':  u'méhesszín', 
        'brl':  u'm1hes5|n', 
    }, {
        'txt':  u'méhesszínben ', 
        'brl':  u'm1hes5|nben ', 
    }, {
        'txt':  u'ménesszárnyékok', 
        'brl':  u'm1nes5"r61kok', 
    }, {
        'txt':  u'méreggyökeret', 
        'brl':  u'm1reg4qkeret', 
    }, {
        'txt':  u'méreggyökérré', 
        'brl':  u'm1reg4qk1rr1', 
    }, {
        'txt':  u'méreggyümölccsé ', 
        'brl':  u'm1reg4{mql331 ', 
    }, {
        'txt':  u'mérleggyár', 
        'brl':  u'm1rleg4"r', 
    }, {
        'txt':  u'meszesszürke', 
        'brl':  u'me5es5{rke', 
    }, {
        'txt':  u'mézsárga', 
        'brl':  u'm12s"rga', 
    }, {
        'txt':  u'mézser', 
        'brl':  u'm12ser', 
    }, {
        'txt':  u'mézsör', 
        'brl':  u'm12sqr', 
    }, {
        'txt':  u'Mikosszéplak', 
        'brl':  u'$mikos51plak', 
    }, {
        'txt':  u'mikrofonnyílás', 
        'brl':  u'mikrofon6|l"s', 
    }, {
        'txt':  u'mikronnyi', 
        'brl':  u'mikron6i', 
    }, {
        'txt':  u'mitesszer', 
        'brl':  u'mite55er', 
    }, {
        'txt':  u'mohaszöld', 
        'brl':  u'mohas2qld', 
    }, {
        'txt':  u'mókusszerű ', 
        'brl':  u'm9kus5er} ', 
    }, {
        'txt':  u'mókusszőr', 
        'brl':  u'm9kus57r', 
    }, {
        'txt':  u'motorosszán', 
        'brl':  u'motoros5"n', 
    }, {
        'txt':  u'mozgásszerű', 
        'brl':  u'mo2g"s5er}', 
    }, {
        'txt':  u'mozgászavar', 
        'brl':  u'mo2g"s2avar', 
    }, {
        'txt':  u'munkásszálló', 
        'brl':  u'munk"s5"ll9', 
    }, {
        'txt':  u'muzsikusszem', 
        'brl':  u'mu`ikus5em', 
    }, {
        'txt':  u'műjéggyár', 
        'brl':  u'm}j1g4"r', 
    }, {
        'txt':  u'működészavar ', 
        'brl':  u'm}kqd1s2avar ', 
    }, {
        'txt':  u'nászutazásszerű ', 
        'brl':  u'n"5uta2"s5er} ', 
    }, {
        'txt':  u'nedvesszürke', 
        'brl':  u'nedves5{rke', 
    }, {
        'txt':  u'négyzetyard', 
        'brl':  u'n142etyard', 
    }, {
        'txt':  u'nehézség', 
        'brl':  u'neh12s1g', 
    }, {
        'txt':  u'nehézsúlyú', 
        'brl':  u'neh12s00', 
    }, {
        'txt':  u'Nemesszalók', 
        'brl':  u'$nemes5al9k', 
    }, {
        'txt':  u'Nemesszentandrás', 
        'brl':  u'$nemes5entandr"s', 
    }, {
        'txt':  u'nemesszőrme', 
        'brl':  u'nemes57rme', 
    }, {
        'txt':  u'nemezsapka', 
        'brl':  u'neme2sapka', 
    }, {
        'txt':  u'nemezsapkát', 
        'brl':  u'neme2sapk"t', 
    }, {
        'txt':  u'nemezsátor ', 
        'brl':  u'neme2s"tor ', 
    }, {
        'txt':  u'nercstóla', 
        'brl':  u'nercst9la', 
    }, {
        'txt':  u'nyargonccsizma', 
        'brl':  u'6argonc3i2ma', 
    }, {
        'txt':  u'nyereggyártók', 
        'brl':  u'6ereg4"rt9k', 
    }, {
        'txt':  u'nyikkanásszerű', 
        'brl':  u'6ikkan"s5er}', 
    }, {
        'txt':  u'nyílászáró', 
        'brl':  u'6|l"s2"r9', 
    }, {
        'txt':  u'nyolcvannyolc', 
        'brl':  u'6olcvan6olc', 
    }, {
        'txt':  u'nyolccsatornás ', 
        'brl':  u'6olc3atorn"s ', 
    }, {
        'txt':  u'nyolcsebességes', 
        'brl':  u'6olcsebess1ges', 
    }, {
        'txt':  u'nyomásszabályzó', 
        'brl':  u'6om"s5ab"29', 
    }, {
        'txt':  u'nyomásszerű', 
        'brl':  u'6om"s5er}', 
    }, {
        'txt':  u'ordasszőrű', 
        'brl':  u'ordas57r}', 
    }, {
        'txt':  u'óriásszalamandra', 
        'brl':  u'9ri"s5alamandra', 
    }, {
        'txt':  u'oroszlánnyom', 
        'brl':  u'oro5l"n6om', 
    }, {
        'txt':  u'orvosszakértő', 
        'brl':  u'orvos5ak1rt7', 
    }, {
        'txt':  u'orvosszázados', 
        'brl':  u'orvos5"2ados', 
    }, {
        'txt':  u'orvosszemély', 
        'brl':  u'orvos5em1', 
    }, {
        'txt':  u'orvosszereikkel', 
        'brl':  u'orvos5ereikkel', 
    }, {
        'txt':  u'orvosszerű', 
        'brl':  u'orvos5er}', 
    }, {
        'txt':  u'orvosszövetség', 
        'brl':  u'orvos5qvets1g', 
    }, {
        'txt':  u'óvodásszintű', 
        'brl':  u'9vod"s5int}', 
    }, {
        'txt':  u'ökrösszekér', 
        'brl':  u'qkrqs5ek1r', 
    }, {
        'txt':  u'önállásszerű', 
        'brl':  u'qn"ll"s5er}', 
    }, {
        'txt':  u'önnyomása,', 
        'brl':  u'qn6om"sa,', 
    }, {
        'txt':  u'önnyomatú ', 
        'brl':  u'qn6omat0 ', 
    }, {
        'txt':  u'örvénylésszerű', 
        'brl':  u'qrv16l1s5er}', 
    }, {
        'txt':  u'ősszármazású ', 
        'brl':  u'7s5"rma2"s0 ', 
    }, {
        'txt':  u'ősszékelyek', 
        'brl':  u'7s51keek', 
    }, {
        'txt':  u'összekéregetett', 
        'brl':  u'q55ek1regetett', 
    }, {
        'txt':  u'összekéregettek', 
        'brl':  u'q55ek1regettek', 
    }, {
        'txt':  u'összekeresgélt', 
        'brl':  u'q55ekeresg1lt', 
    }, {
        'txt':  u'ősszel', 
        'brl':  u'755el', 
    }, {
        'txt':  u'ősszellem', 
        'brl':  u'7s5ellem', 
    }, {
        'txt':  u'őzsebesen', 
        'brl':  u'72sebesen', 
    }, {
        'txt':  u'őzsörét', 
        'brl':  u'72sqr1t', 
    }, {
        'txt':  u'őzsuta', 
        'brl':  u'72suta', 
    }, {
        'txt':  u'padlásszoba', 
        'brl':  u'padl"s5oba', 
    }, {
        'txt':  u'padlászugoly', 
        'brl':  u'padl"s2ugo', 
    }, {
        'txt':  u'palócság', 
        'brl':  u'pal9cs"g', 
    }, {
        'txt':  u'papirosszeletre', 
        'brl':  u'papiros5eletre', 
    }, {
        'txt':  u'paprikásszalonna-bazár', 
        'brl':  u'paprik"s5alonna-ba2"r', 
    }, {
        'txt':  u'papucszápor', 
        'brl':  u'papu32"por', 
    }, {
        'txt':  u'párnásszék', 
        'brl':  u'p"rn"s51k', 
    }, {
        'txt':  u'pátosszal', 
        'brl':  u'p"to55al', 
    }, {
        'txt':  u'pedagógusszervezet', 
        'brl':  u'pedag9gus5erve2et', 
    }, {
        'txt':  u'pedagógusszobába', 
        'brl':  u'pedag9gus5ob"ba', 
    }, {
        'txt':  u'pedagógussztrájk', 
        'brl':  u'pedag9gus5tr"jk', 
    }, {
        'txt':  u'pénzeszacskó', 
        'brl':  u'p1n2es2a3k9', 
    }, {
        'txt':  u'pénzeszsák', 
        'brl':  u'p1n2es`"k', 
    }, {
        'txt':  u'pénzzsidóságban ', 
        'brl':  u'p1n2`id9s"gban ', 
    }, {
        'txt':  u'pénzsegély', 
        'brl':  u'p1n2seg1', 
    }, {
        'txt':  u'pénzsorozat', 
        'brl':  u'p1n2soro2at', 
    }, {
        'txt':  u'pénzsóvár', 
        'brl':  u'p1n2s9v"r', 
    }, {
        'txt':  u'pénzszűke', 
        'brl':  u'p1n25}ke', 
    }, {
        'txt':  u'perecsütőnő', 
        'brl':  u'perecs{t7n7', 
    }, {
        'txt':  u'pergamennyaláb', 
        'brl':  u'pergamen6al"b', 
    }, {
        'txt':  u'piaccsarnok', 
        'brl':  u'piac3arnok', 
    }, {
        'txt':  u'Pilisszántó,', 
        'brl':  u'$pilis5"nt9,', 
    }, {
        'txt':  u'Pilisszentkereszt', 
        'brl':  u'$pilis5entkere5t', 
    }, {
        'txt':  u'pirítósszeleteken', 
        'brl':  u'pir|t9s5eleteken', 
    }, {
        'txt':  u'pirosszem', 
        'brl':  u'piros5em', 
    }, {
        'txt':  u'piroszászlós', 
        'brl':  u'piros2"5l9s', 
    }, {
        'txt':  u'piszkosszőke', 
        'brl':  u'pi5kos57ke', 
    }, {
        'txt':  u'piszkosszürke ', 
        'brl':  u'pi5kos5{rke ', 
    }, {
        'txt':  u'piszkoszöld', 
        'brl':  u'pi5kos2qld', 
    }, {
        'txt':  u'pisztolylövésszerűen', 
        'brl':  u'pi5tolqv1s5er}en', 
    }, {
        'txt':  u'plüsszerű', 
        'brl':  u'pl{s5er}', 
    }, {
        'txt':  u'plüsszsák', 
        'brl':  u'pl{ss`"k', 
    }, {
        'txt':  u'plüsszsiráf ', 
        'brl':  u'pl{ss`ir"f ', 
    }, {
        'txt':  u'plüsszsölyét', 
        'brl':  u'pl{ss`q1t', 
    }, {
        'txt':  u'pokróccsík', 
        'brl':  u'pokr9c3|k', 
    }, {
        'txt':  u'polcsor', 
        'brl':  u'polcsor', 
    }, {
        'txt':  u'policájkomisszér', 
        'brl':  u'polic"jkomi551r', 
    }, {
        'txt':  u'porcsérülés', 
        'brl':  u'porcs1r{l1s', 
    }, {
        'txt':  u'portásszoba', 
        'brl':  u'port"s5oba', 
    }, {
        'txt':  u'postásszakszervezet', 
        'brl':  u'post"s5ak5erve2et', 
    }, {
        'txt':  u'precízség', 
        'brl':  u'prec|2s1g', 
    }, {
        'txt':  u'présszerű', 
        'brl':  u'pr1s5er}', 
    }, {
        'txt':  u'priamossza', 
        'brl':  u'priamo55a', 
    }, {
        'txt':  u'princséged', 
        'brl':  u'princs1ged', 
    }, {
        'txt':  u'proggyak', 
        'brl':  u'prog4ak', 
    }, {
        'txt':  u'pulzusszám', 
        'brl':  u'pul2us5"m', 
    }, {
        'txt':  u'puskásszakasz', 
        'brl':  u'pusk"s5aka5', 
    }, {
        'txt':  u'puskásszázad', 
        'brl':  u'pusk"s5"2ad', 
    }, {
        'txt':  u'puskászászlóalj ', 
        'brl':  u'pusk"s2"5l9alj ', 
    }, {
        'txt':  u'rácság', 
        'brl':  u'r"cs"g', 
    }, {
        'txt':  u'rádiósszoba', 
        'brl':  u'r"di9s5oba', 
    }, {
        'txt':  u'rakásszámra', 
        'brl':  u'rak"s5"mra', 
    }, {
        'txt':  u'rándulásszerűen', 
        'brl':  u'r"ndul"s5er}en', 
    }, {
        'txt':  u'rántásszag', 
        'brl':  u'r"nt"s5ag', 
    }, {
        'txt':  u'rántásszerű ', 
        'brl':  u'r"nt"s5er} ', 
    }, {
        'txt':  u'recésszárú', 
        'brl':  u'rec1s5"r0', 
    }, {
        'txt':  u'régiséggyűjtő', 
        'brl':  u'r1gis1g4}jt7', 
    }, {
        'txt':  u'rémisszétek', 
        'brl':  u'r1mi551tek', 
    }, {
        'txt':  u'rendelkezésszerű', 
        'brl':  u'rendelke21s5er}', 
    }, {
        'txt':  u'repülésszerű', 
        'brl':  u'rep{l1s5er}', 
    }, {
        'txt':  u'repülősszárny', 
        'brl':  u'rep{l7s5"r6', 
    }, {
        'txt':  u'rézsut', 
        'brl':  u'r1`ut', 
    }, {
        'txt':  u'ritkasággyűjtő', 
        'brl':  u'ritkas"g4}jt7', 
    }, {
        'txt':  u'ritmusszabályozó', 
        'brl':  u'ritmus5ab"o29', 
    }, {
        'txt':  u'ritmuszavar', 
        'brl':  u'ritmus2avar', 
    }, {
        'txt':  u'robbanásszerű', 
        'brl':  u'robban"s5er}', 
    }, {
        'txt':  u'robbanászajt', 
        'brl':  u'robban"s2ajt', 
    }, {
        'txt':  u'rongyosszélű', 
        'brl':  u'ron4os51l}', 
    }, {
        'txt':  u'rosszemlékű', 
        'brl':  u'ro55eml1k}', 
    }, {
        'txt':  u'rothadásszag', 
        'brl':  u'rothad"s5ag', 
    }, {
        'txt':  u'rózsásszőkés', 
        'brl':  u'r9`"s57k1s', 
    }, {
        'txt':  u'rozszabálás', 
        'brl':  u'ro`2ab"l"s', 
    }, {
        'txt':  u'rubinnyaklánc', 
        'brl':  u'rubin6akl"nc', 
    }, {
        'txt':  u'ruhásszekrény', 
        'brl':  u'ruh"s5ekr16', 
    }, {
        'txt':  u'ruhásszobámé', 
        'brl':  u'ruh"s5ob"m1', 
    }, {
        'txt':  u'sárgásszínű', 
        'brl':  u's"rg"s5|n}', 
    }, {
        'txt':  u'sasszárnyú', 
        'brl':  u'sas5"r60', 
    }, {
        'txt':  u'sasszeg', 
        'brl':  u'sas5eg', 
    }, {
        'txt':  u'sásszéna', 
        'brl':  u's"s51na', 
    }, {
        'txt':  u'sásszerű ', 
        'brl':  u's"s5er} ', 
    }, {
        'txt':  u'sasszömöd', 
        'brl':  u'sas5qmqd', 
    }, {
        'txt':  u'sátáncsapat', 
        'brl':  u's"t"n3apat', 
    }, {
        'txt':  u'selymesszőke', 
        'brl':  u'semes57ke', 
    }, {
        'txt':  u'sereggyűjtés', 
        'brl':  u'sereg4}jt1s', 
    }, {
        'txt':  u'sertésszelet', 
        'brl':  u'sert1s5elet', 
    }, {
        'txt':  u'sertésszűzpecsenyére', 
        'brl':  u'sert1s5}2pe3e61re', 
    }, {
        'txt':  u'sertészsír', 
        'brl':  u'sert1s`|r', 
    }, {
        'txt':  u'sonkásszelet', 
        'brl':  u'sonk"s5elet', 
    }, {
        'txt':  u'sonkásszerű ', 
        'brl':  u'sonk"s5er} ', 
    }, {
        'txt':  u'sonkászsemle', 
        'brl':  u'sonk"s`emle', 
    }, {
        'txt':  u'sorscsapásszerű,', 
        'brl':  u'sors3ap"s5er},', 
    }, {
        'txt':  u'spanyolmeggyen', 
        'brl':  u'spa6olme44en', 
    }, {
        'txt':  u'sugárzásszerű ', 
        'brl':  u'sug"r2"s5er} ', 
    }, {
        'txt':  u'sugárzásszintek', 
        'brl':  u'sug"r2"s5intek', 
    }, {
        'txt':  u'suhanccsürhe', 
        'brl':  u'suhanc3{rhe', 
    }, {
        'txt':  u'suhancsereg', 
        'brl':  u'suhancsereg', 
    }, {
        'txt':  u'szaglásszék', 
        'brl':  u'5agl"551k', 
    }, {
        'txt':  u'szalonnyelv, szalonnyelven ', 
        'brl':  u'5alon6elv, 5alon6elven ', 
    }, {
        'txt':  u'Szamosszeg', 
        'brl':  u'$5amos5eg', 
    }, {
        'txt':  u'szanitéccsoport', 
        'brl':  u'5anit1c3oport', 
    }, {
        'txt':  u'szárazság', 
        'brl':  u'5"ra2s"g', 
    }, {
        'txt':  u'szárazsült', 
        'brl':  u'5"ra2s{lt', 
    }, {
        'txt':  u'szarvasszív', 
        'brl':  u'5arvas5|v', 
    }, {
        'txt':  u'szarvaszsír', 
        'brl':  u'5arvas`|r', 
    }, {
        'txt':  u'szénásszekér', 
        'brl':  u'51n"s5ek1r', 
    }, {
        'txt':  u'szentségesszűzmáriám', 
        'brl':  u'5ents1ges5}2m"ri"m', 
    }, {
        'txt':  u'szentséggyalázás', 
        'brl':  u'5ents1g4al"2"s', 
    }, {
        'txt':  u'Szepesszombat', 
        'brl':  u'$5epes5ombat', 
    }, {
        'txt':  u'szerződésszegés', 
        'brl':  u'5er27d1s5eg1s', 
    }, {
        'txt':  u'szerződésszegő ', 
        'brl':  u'5er27d1s5eg7 ', 
    }, {
        'txt':  u'szétossza', 
        'brl':  u'51to55a', 
    }, {
        'txt':  u'szétosszák ', 
        'brl':  u'51to55"k ', 
    }, {
        'txt':  u'Szilvásszentmárton', 
        'brl':  u'$5ilv"s5entm"rton', 
    }, {
        'txt':  u'szindikátusszerű ', 
        'brl':  u'5indik"tus5er} ', 
    }, {
        'txt':  u'szindikátusszervező', 
        'brl':  u'5indik"tus5erve27', 
    }, {
        'txt':  u'színnyomás', 
        'brl':  u'5|n6om"s', 
    }, {
        'txt':  u'szocsegély', 
        'brl':  u'5ocseg1', 
    }, {
        'txt':  u'szódásszifon', 
        'brl':  u'59d"s5ifon', 
    }, {
        'txt':  u'szokásszerű', 
        'brl':  u'5ok"s5er}', 
    }, {
        'txt':  u'szólásszabadság', 
        'brl':  u'59l"s5abads"g', 
    }, {
        'txt':  u'szólásszapulás', 
        'brl':  u'59l"s5apul"s', 
    }, {
        'txt':  u'szórakozásszámba', 
        'brl':  u'59rako2"s5"mba', 
    }, {
        'txt':  u'szorongásszerű', 
        'brl':  u'5orong"s5er}', 
    }, {
        'txt':  u'szorongásszint', 
        'brl':  u'5orong"s5int', 
    }, {
        'txt':  u'szöggyár', 
        'brl':  u'5qg4"r', 
    }, {
        'txt':  u'szöveggyűjtemény', 
        'brl':  u'5qveg4}jtem16', 
    }, {
        'txt':  u'szúrásszerű', 
        'brl':  u'50r"s5er}', 
    }, {
        'txt':  u'szúrósszemű', 
        'brl':  u'50r9s5em}', 
    }, {
        'txt':  u'születésszabályozás', 
        'brl':  u'5{let1s5ab"o2"s', 
    }, {
        'txt':  u'szűzsült', 
        'brl':  u'5}2s{lt', 
    }, {
        'txt':  u'tágulásszabály', 
        'brl':  u't"gul"s5ab"', 
    }, {
        'txt':  u'taggyűlés', 
        'brl':  u'tag4}l1s', 
    }, {
        'txt':  u'tánccsoport', 
        'brl':  u't"nc3oport', 
    }, {
        'txt':  u'társalgásszámba', 
        'brl':  u't"rsalg"s5"mba', 
    }, {
        'txt':  u'társalgásszerű ', 
        'brl':  u't"rsalg"s5er} ', 
    }, {
        'txt':  u'társszekér', 
        'brl':  u't"rs5ek1r', 
    }, {
        'txt':  u'társszövetség', 
        'brl':  u't"rs5qvets1g', 
    }, {
        'txt':  u'teásszervíz', 
        'brl':  u'te"s5erv|2', 
    }, {
        'txt':  u'tejfölösszájú', 
        'brl':  u'tejfqlqs5"j0', 
    }, {
        'txt':  u'településszerkezet ', 
        'brl':  u'telep{l1s5erke2et ', 
    }, {
        'txt':  u'templomosszakértő', 
        'brl':  u'templomos5ak1rt7', 
    }, {
        'txt':  u'tetszészaj', 
        'brl':  u'tet51s2aj', 
    }, {
        'txt':  u'tetszészsivaj ', 
        'brl':  u'tet51s`ivaj ', 
    }, {
        'txt':  u'tigrisszemek ', 
        'brl':  u'tigris5emek ', 
    }, {
        'txt':  u'tigrisszerű', 
        'brl':  u'tigris5er}', 
    }, {
        'txt':  u'tipusszám', 
        'brl':  u'tipus5"m', 
    }, {
        'txt':  u'típusszám', 
        'brl':  u't|pus5"m', 
    }, {
        'txt':  u'típusszöveg', 
        'brl':  u't|pus5qveg', 
    }, {
        'txt':  u'típuszubbony', 
        'brl':  u't|pus2ubbo6', 
    }, {
        'txt':  u'titkosszolgálat', 
        'brl':  u'titkos5olg"lat', 
    }, {
        'txt':  u'tizedesszállás', 
        'brl':  u'ti2edes5"ll"s', 
    }, {
        'txt':  u'tízgallonnyi', 
        'brl':  u't|2gallon6i', 
    }, {
        'txt':  u'tojásszerű', 
        'brl':  u'toj"s5er}', 
    }, {
        'txt':  u'topázsárgája', 
        'brl':  u'top"2s"rg"ja', 
    }, {
        'txt':  u'torzság', 
        'brl':  u'tor2s"g', 
    }, {
        'txt':  u'többesszám', 
        'brl':  u'tqbbes5"m', 
    }, {
        'txt':  u'töltésszabályozó', 
        'brl':  u'tqlt1s5ab"o29', 
    }, {
        'txt':  u'töltésszám', 
        'brl':  u'tqlt1s5"m', 
    }, {
        'txt':  u'töltésszerűen', 
        'brl':  u'tqlt1s5er}en', 
    }, {
        'txt':  u'törlesszem', 
        'brl':  u'tqrle55em', 
    }, {
        'txt':  u'tövisszár', 
        'brl':  u'tqvis5"r', 
    }, {
        'txt':  u'tövisszúrás', 
        'brl':  u'tqvis50r"s', 
    }, {
        'txt':  u'trichinnyavalya', 
        'brl':  u'trichin6avaa', 
    }, {
        'txt':  u'tudásszint', 
        'brl':  u'tud"s5int', 
    }, {
        'txt':  u'tudásszomj', 
        'brl':  u'tud"s5omj', 
    }, {
        'txt':  u'tükrösszélű', 
        'brl':  u't{krqs51l}', 
    }, {
        'txt':  u'tüntetésszerűen', 
        'brl':  u't{ntet1s5er}en', 
    }, {
        'txt':  u'tüzesszemű', 
        'brl':  u't{2es5em}', 
    }, {
        'txt':  u'tűzzsonglőr ', 
        'brl':  u't}2`ongl7r ', 
    }, {
        'txt':  u'tűzsebesség', 
        'brl':  u't}2sebess1g', 
    }, {
        'txt':  u'tűzsugár', 
        'brl':  u't}2sug"r', 
    }, {
        'txt':  u'udvaroncsereg', 
        'brl':  u'udvaroncsereg', 
    }, {
        'txt':  u'ugrásszerűen', 
        'brl':  u'ugr"s5er}en', 
    }, {
        'txt':  u'újoncság', 
        'brl':  u'0joncs"g', 
    }, {
        'txt':  u'ulánusszázad', 
        'brl':  u'ul"nus5"2ad', 
    }, {
        'txt':  u'university', 
        'brl':  u'university', 
    }, {
        'txt':  u'utalásszerűen', 
        'brl':  u'utal"s5er}en', 
    }, {
        'txt':  u'utánpótlásszállítmánnyal', 
        'brl':  u'ut"np9tl"s5"ll|tm"66al', 
    }, {
        'txt':  u'utánnyomás', 
        'brl':  u'ut"n6om"s', 
    }, {
        'txt':  u'utasszállító', 
        'brl':  u'utas5"ll|t9', 
    }, {
        'txt':  u'utasszám', 
        'brl':  u'utas5"m', 
    }, {
        'txt':  u'utasszerep', 
        'brl':  u'utas5erep', 
    }, {
        'txt':  u'utasszint', 
        'brl':  u'utas5int', 
    }, {
        'txt':  u'utasszolgálat', 
        'brl':  u'utas5olg"lat', 
    }, {
        'txt':  u'utazásszerű', 
        'brl':  u'uta2"s5er}', 
    }, {
        'txt':  u'utazásszervező', 
        'brl':  u'uta2"s5erve27', 
    }, {
        'txt':  u'ülésszak', 
        'brl':  u'{l1s5ak', 
    }, {
        'txt':  u'ülésszak', 
        'brl':  u'{l1s5ak', 
    }, {
        'txt':  u'ütésszerű', 
        'brl':  u'{t1s5er}', 
    }, {
        'txt':  u'ütészápor ', 
        'brl':  u'{t1s2"por ', 
    }, {
        'txt':  u'üveggyapot', 
        'brl':  u'{veg4apot', 
    }, {
        'txt':  u'üveggyártás', 
        'brl':  u'{veg4"rt"s', 
    }, {
        'txt':  u'üveggyöngy', 
        'brl':  u'{veg4qn4', 
    }, {
        'txt':  u'üveggyűrű', 
        'brl':  u'{veg4}r}', 
    }, {
        'txt':  u'vagonnyi', 
        'brl':  u'vagon6i', 
    }, {
        'txt':  u'vakarccsal', 
        'brl':  u'vakar33al', 
    }, {
        'txt':  u'vallásszabadság', 
        'brl':  u'vall"s5abads"g', 
    }, {
        'txt':  u'vallomásszámba', 
        'brl':  u'vallom"s5"mba', 
    }, {
        'txt':  u'vállpereccsontjából', 
        'brl':  u'v"llperec3ontj"b9l', 
    }, {
        'txt':  u'Vámosszabadi', 
        'brl':  u'$v"mos5abadi', 
    }, {
        'txt':  u'városszépe', 
        'brl':  u'v"ros51pe', 
    }, {
        'txt':  u'városszerte', 
        'brl':  u'v"ros5erte', 
    }, {
        'txt':  u'várossziluett', 
        'brl':  u'v"ros5iluett', 
    }, {
        'txt':  u'városzajon', 
        'brl':  u'v"ros2ajon', 
    }, {
        'txt':  u'városzajt ', 
        'brl':  u'v"ros2ajt ', 
    }, {
        'txt':  u'városzsarnok', 
        'brl':  u'v"ros`arnok', 
    }, {
        'txt':  u'vasasszakosztály', 
        'brl':  u'vasas5ako5t"', 
    }, {
        'txt':  u'vasutassztrájk', 
        'brl':  u'vasutas5tr"jk', 
    }, {
        'txt':  u'vasszállítmány', 
        'brl':  u'vas5"ll|tm"6', 
    }, {
        'txt':  u'Vaszar', 
        'brl':  u'$va5ar', 
    }, {
        'txt':  u'Vázsnok', 
        'brl':  u'$v"`nok', 
    }, {
        'txt':  u'végiggyakorolni', 
        'brl':  u'v1gig4akorolni', 
    }, {
        'txt':  u'végiggyalogolhatja', 
        'brl':  u'v1gig4alogolhatja', 
    }, {
        'txt':  u'vendéggyermek', 
        'brl':  u'vend1g4ermek', 
    }, {
        'txt':  u'vendéggyülekezet', 
        'brl':  u'vend1g4{leke2et', 
    }, {
        'txt':  u'veresszakállú ', 
        'brl':  u'veres5ak"ll0 ', 
    }, {
        'txt':  u'veresszemű', 
        'brl':  u'veres5em}', 
    }, {
        'txt':  u'versszak', 
        'brl':  u'vers5ak', 
    }, {
        'txt':  u'versszakában ', 
        'brl':  u'vers5ak"ban ', 
    }, {
        'txt':  u'versszerű', 
        'brl':  u'vers5er}', 
    }, {
        'txt':  u'vértesszázad', 
        'brl':  u'v1rtes5"2ad', 
    }, {
        'txt':  u'vértesszázadbeli ', 
        'brl':  u'v1rtes5"2adbeli ', 
    }, {
        'txt':  u'Vértesszőlős', 
        'brl':  u'$v1rtes57l7s', 
    }, {
        'txt':  u'vetésszalag', 
        'brl':  u'vet1s5alag', 
    }, {
        'txt':  u'viccsor', 
        'brl':  u'viccsor', 
    }, {
        'txt':  u'világossággyújtásra', 
        'brl':  u'vil"goss"g40jt"sra', 
    }, {
        'txt':  u'világosszőke', 
        'brl':  u'vil"gos57ke', 
    }, {
        'txt':  u'világosszürke ', 
        'brl':  u'vil"gos5{rke ', 
    }, {
        'txt':  u'világgyűlölő', 
        'brl':  u'vil"g4}lql7', 
    }, {
        'txt':  u'villamosszék', 
        'brl':  u'villamos51k', 
    }, {
        'txt':  u'villamosszerű', 
        'brl':  u'villamos5er}', 
    }, {
        'txt':  u'villanásszerűen', 
        'brl':  u'villan"s5er}en', 
    }, {
        'txt':  u'virággyapjaikat', 
        'brl':  u'vir"g4apjaikat', 
    }, {
        'txt':  u'virággyűjtemény ', 
        'brl':  u'vir"g4}jtem16 ', 
    }, {
        'txt':  u'virággyűjtés', 
        'brl':  u'vir"g4}jt1s', 
    }, {
        'txt':  u'virgoncság', 
        'brl':  u'virgoncs"g', 
    }, {
        'txt':  u'vírusszerű', 
        'brl':  u'v|rus5er}', 
    }, {
        'txt':  u'viselkedésszerű', 
        'brl':  u'viselked1s5er}', 
    }, {
        'txt':  u'viselkedészavar', 
        'brl':  u'viselked1s2avar', 
    }, {
        'txt':  u'visually', 
        'brl':  u'visually', 
    }, {
        'txt':  u'visszér', 
        'brl':  u'vi551r', 
    }, {
        'txt':  u'visszeres', 
        'brl':  u'vi55eres', 
    }, {
        'txt':  u'visszérműtét ', 
        'brl':  u'vi551rm}t1t ', 
    }, {
        'txt':  u'vitorlásszezon', 
        'brl':  u'vitorl"s5e2on', 
    }, {
        'txt':  u'vizeleteszacskó ', 
        'brl':  u'vi2eletes2a3k9 ', 
    }, {
        'txt':  u'vizeszsemle', 
        'brl':  u'vi2es`emle', 
    }, {
        'txt':  u'vízsáv', 
        'brl':  u'v|2s"v', 
    }, {
        'txt':  u'vízsivatag ', 
        'brl':  u'v|2sivatag ', 
    }, {
        'txt':  u'vízsodor', 
        'brl':  u'v|2sodor', 
    }, {
        'txt':  u'vonósszerenád ', 
        'brl':  u'von9s5eren"d ', 
    }, {
        'txt':  u'vonószenekar', 
        'brl':  u'von9s2enekar', 
    }, {
        'txt':  u'vörhenyesszőke', 
        'brl':  u'vqrhe6es57ke', 
    }, {
        'txt':  u'vörösesszőke', 
        'brl':  u'vqrqses57ke', 
    }, {
        'txt':  u'vörösesszőke', 
        'brl':  u'vqrqses57ke', 
    }, {
        'txt':  u'vörösszakállú', 
        'brl':  u'vqrqs5ak"ll0', 
    }, {
        'txt':  u'vörösszem', 
        'brl':  u'vqrqs5em', 
    }, {
        'txt':  u'vörösszínű', 
        'brl':  u'vqrqs5|n}', 
    }, {
        'txt':  u'vörösszőke', 
        'brl':  u'vqrqs57ke', 
    }, {
        'txt':  u'vöröszászló', 
        'brl':  u'vqrqs2"5l9', 
    }, {
        'txt':  u'záloggyűrűmet', 
        'brl':  u'2"log4}r}met', 
    }, {
        'txt':  u'zavarosszürke', 
        'brl':  u'2avaros5{rke', 
    }, {
        'txt':  u'zökkenésszerű', 
        'brl':  u'2qkken1s5er}', 
    }, {
        'txt':  u'zörgésszerű', 
        'brl':  u'2qrg1s5er}', 
    }, {
        'txt':  u'zuhanásszerű', 
        'brl':  u'2uhan"s5er}', 
    }, {
        'txt':  u'zsírosszén', 
        'brl':  u'`|ros51n', 
    }, {
        'txt':  u'zsoldosszokás', 
        'brl':  u'`oldos5ok"s', 
    },
    #Testing a sentence with a phone number. Braille output should have numberSign after dashes.
    {
        'txt':  u'A telefonszámom: 06-1-256-256.', 
        'brl':  u'$a telefon5"mom: #jf-#a-#bef-#bef.', 
    },
    #Testing a decimal number containing sentence. After the comma character not need have numbersign indicator.
    {
        'txt':  u'A földrengés 7,5 erősségű volt.', 
        'brl':  u'$a fqldreng1s #g,e er7ss1g} volt.', 
    }, {
        'txt':  u'Tamás még nem tudta, hogy mi vár rá.', 
        'brl':  u'$tam"s m1g nem tudta, ho4 mi v"r r".', 
    },
    #Testing a Quotation mark containing sentence. Two quotation mark characters need resulting different dot combinations.
    #Left quotation mark dot combination is 236, right quotation mark dot combination is 356.
    {
        'txt':  u'"Tamás még nem tudta, hogy mi vár rá."', 
        'brl':  u'($tam"s m1g nem tudta, ho4 mi v"r r".)', 
    },
    #Testing a sentance containing an apostrophe. The right dot combination is dots 6-3.
    {
        'txt':  u"I don't no why happened this problem.", 
        'brl':  u"$i don'.t no why happened this problem.", 
    },
    #Testing a § (paragraph mark character) containing sentence. The paragraph mark character dot combination with hungarian language is 3456-1236.
    {
        'txt':  u'Az 1§ paragrafus alapján a kedvezmény igénybe vehető.', 
        'brl':  u'$a2 #a#v paragrafus alapj"n a kedve2m16 ig16be vehet7.', 
    },
    #testing some sentences with containing – character from a book.
    {
        'txt':  u'– A sivatagi karavánúton, negyven vagy ötven mérföldnyire innen. Egy Ford. De mi nem megyünk magával. – Elővette az irattárcáját, és átadott Naszifnak egy angol fontot. – Ha visszajön, keressen meg a vasútállomás mellett a Grand Hotelban.', 
        'brl':  u'- $a sivatagi karav"n0ton, ne4ven va4 qtven m1rfqld6ire innen. $e4 $ford. $de mi nem me4{nk mag"val. - $el7vette a2 iratt"rc"j"t, 1s "tadott $na5ifnak e4 angol fontot. - $ha vi55ajqn, keressen meg a vas0t"llom"s mellett a $grand $hotelban.', 
    },
    #Testing euro dot combination. The € character right dot combination is 56-15.
    {
        'txt':  u'A vételár 1 €.', 
        'brl':  u'$a v1tel"r #a <e.', 
    },
    #Testing left and right parentheses dot combinations. Left parenthese dot combination is 2346, right parenthese dot combination is 1356.
    {
        'txt':  u'De nekem nagyon nehezemre esik a levélírás (nézd el a helyesírási hibákat és a csúnya írásomat).', 
        'brl':  u'$de nekem na4on nehe2emre esik a lev1l|r"s ~n12d el a hees|r"si hib"kat 1s a 306a |r"somatz.', 
    },
    #Testing capsign indicator. If a word entire containing uppercase letter, need marking this with two 46 dot combination before the word.
    {
        'txt':  u'A kiállítás megnyitóján jelen volt az MVGYOSZ elnöke.', 
        'brl':  u'$a ki"ll|t"s meg6it9j"n jelen volt a2 $$mv4o5 elnqke.', 
    },
    #Testing time format. If a sentence containing 11:45 style text part, need replacing the colon 25 dot combination with dot3 combination, and not need marking numbersign indicator the next numbers.
    {
        'txt':  u'A pontos idő 11:45 perc.', 
        'brl':  u'$a pontos id7 #aa.de perc.', 
    },
    #Testing bulleted list item, the right dot combination is 3-35.
    {
        'txt':  u'•A kiadás újdonságai.', 
        'brl':  u"""'*$a kiad"s 0jdons"gai.""", 
    },
    #Testing underscore, the right dot combination is 6-36.
    {
        'txt':  u'A hu_list fájl nem létezik.', 
        'brl':  u"""$a hu'-list f"jl nem l1te2ik.""", 
    },
    #Testing a sentance containing an e-mail address, the dot combination for at sign is dots 45
    {
        'txt':  u'Az e-mail címem: akarmi@akarmi.hu.', 
        'brl':  u'$a2 e-mail c|mem: akarmi>akarmi.hu.', 
    },
]
