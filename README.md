# Hardwarové věci k etapě InTime 2025
Tento repozitář vznikl pro uchování hardwarových věcí potřebných k táborové etapě skautského oddílu Ontario - InTime 2025. Tvoří jeden celek se softwarovým řešením systému pro uchovávání časů, uživatelů a tagů, který neleznete zde: https://github.com/ontarioskaut/tabor2025. Detailnější informace jsou k mání na naši skautské wiki: https://wiki.zlin6.cz/cs/technika/intime2025.

## Obecná probematika
Pro celotáborovou hru jsme se rozhodli udělat systém, vekterém dominuje odpočítávaný čas jako měna. Členi mají RFID náramky a v dílčích částech tábora mají možnost dostat kulaté RFID tagy jakožto coiny s nějakou hodnotou. Mají jak zisky, tak výdaje za různé věci a snaží se s tím hospodařit. Aby to fungovalo, použili jsme moderní technologie a naprogramovali časový server, který řeší management uživatelů, náramků, coinů. Prostě nadstavba s API a GUI nad databází.

Tento server běží na mikropočítači (zde použit ROCK PI S s druhou WiFi kartou) a vše komunikuje bezdrátově přes WiFi. Vedoucí mají možnost zasahovat do systému skrze webové prostředí nebo webovou aplikaci používající přímo NFC API v prohlížeči, členi mohou managovat svůj čas skrze time terminals (ty umožňují platit, převádět, skenovat coiny a zjišťovat zůstatek). Terminály mají uvnitř RFID čtečku a ESP32, přes které se hardware řídí a je prováděna komunikace s časovým serverem skrze REST API. 

Další částí jsou displeje pro zobrazování aktuálních časů hráčů. K tomu jsme vybrali vyřazené Buse 210 displeje z autobusů, protože jsou velké, vypadají cool a jsou oproti LED variantám velice energeticky úsporné. Tam byl problém se zobrazováním vlastních dat, protože se pro komunikaci používá sběrnice IBIS (sériová sběrnice posazená na 24V s pomalým baudratem) s uzavřeným komunikačním protokolem. Prostě technologie někdy z 80. let. Vzhledem k tomu, že se v původních verzích panelů ani nepočítalo s vlastní grafikou, která bude často updatovaná (v panelech je databáze předpřipravených obrazovek, pomocí příkazů se přepínají). Vznikla tedy potřeba vlastního řízení. Zde nám usnadnil práci člově, který stojí za Flipity 210: https://github.com/mcer12/Flippity210. Pro naše účely stačilo naprogramovat ESP01S modulek s firmwarem, který periodicky stahuje předpřipravené obrazovky z time serveru a posílá je k zobrazení.

## displays/display_tool
- GTK aplikace sloužící k roční přípravě a testování zobrazení na Buse210 displejích. Aplikace generuje base64-encoded string, který obsahuje obrazové a adresní informace a slouží k ovládání řídící desky u displejů. 
- Existují dvě modifikace:
  - display_tool.py - tato sloužila k sériovému ovládání displejů. I2C sběrnice s displejů 
  - display_tool_web.py - modifikace, která plive data kompatibilní s webovou verzí editoru. Autorovi se totiž líp pracuje v desktopové verzi a navíc má lepší řešení low_resolution vykreslování fontů.
  
## time_terminal
- Veškeré podklady k hardwaru time_terminalu vytvořené v KiCadu. Jsou tam vytvořené přímo manifacturing data, ale ty silně nedoporučuji používat, protože design desky obsahuje mnoho chyb, které bylo potřeba manuálně předrátovat. Jedná se hlavně o špatné použití pinů, které jsou připojeny k interní flashce, takže to způsobovalo problémy u programování. Potom jsou někte použity input only piny pro output a tak dále. Bohužel si autor při kreslení schémat nedostatečně přečetl dokumentaci ESP32 modulu a pak se divil.

## firmware/esp8266_buse_cient
- Firmware pro esp01S připojené do Flipity 210 desky u displeje. Tato verze se používá ve finálním stavu systému - prostě vše se renderuje na serveru a esp jenom periodicky posílá requesty a stahuje base64-encoded obrázky, které jen vyplivne na displej.
- Pro správní fungování na největší (čelní) variantě displeje s 5 flip-dot panely byl ve Flipity210 firmwaru pozměněn limit panelů a nastavený jinší count v sekci čtení konfiguračních jumperů.
- Nevím, jak je to ve vyšší verzi Flipity210 hardwaru, ale ve verzi 2.1 je potřeba dávat pozor na voltage spiky způsobené čínským step-down modulem. Na tomto jsem při testování spálil pár STM32 procesorů. Já to řešil pomocí přidání velkého kondenzátoru a TVS na výstup. 

## firmware/ostatní
- Předchzí iterace způsobené nedostatkem espéček v mém sklepě. Jednalo se o aloučení I2C a společné řízení a potom na delší vzdálenosti použití sériového protokolu, pomocí kterého se dalo řešit posílání přímo z počítače s převodníkem.
