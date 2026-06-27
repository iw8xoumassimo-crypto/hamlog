#include "dxcc.h"
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <algorithm>

namespace Dxcc {

// ---------------------------------------------------------------------------
// DXCC entity table
// ---------------------------------------------------------------------------
struct Entity {
    QString name;
    int     cqZone;
    int     ituZone;
    QString continent;
    double  lat;
    double  lon;
    bool    deleted;
};

// prefix -> Entity  (primary prefixes and overrides)
static QHash<QString,Entity> s_map;
// sorted prefixes (longest first) for greedy match
static QStringList           s_keys;
static QMutex                s_mutex;

static void buildTable()
{
    // FORMAT: { "PREFIX(ES)|...", {name, cqZone, ituZone, cont, lat, lon, deleted} }
    // Each prefix string may contain multiple variants separated by '|'.
    // Exception prefixes start with '!' and override a base prefix.
    // This table mirrors the Python dxcc_data dict used in the original project.

    struct Row { const char* prefixes; Entity ent; };

    static const Row table[] = {
        // callarea prefixes like KL, VK, etc. come after continent blocks

        // North America
        {"K|W|N|AA-AK|KA-KZ|NA-NZ|WA-WZ",{"United States",5,8,"NA",38,-97,false}},
        {"VE|VA|VB|VC|VX|VY",{"Canada",4,3,"NA",60,-95,false}},
        {"XE|XF",{"Mexico",7,10,"NA",19,-102,false}},
        {"TI",{"Costa Rica",7,11,"NA",10,-84,false}},
        {"HP",{"Panama",7,11,"NA",9,-79,false}},
        {"YN",{"Nicaragua",7,11,"NA",13,-85,false}},
        {"HQ",{"Honduras",7,11,"NA",15,-87,false}},
        {"TG",{"Guatemala",7,11,"NA",15,-90,false}},
        {"YS",{"El Salvador",7,11,"NA",14,-89,false}},
        {"V3",{"Belize",7,11,"NA",17,-89,false}},
        {"FG",{"Guadeloupe",8,11,"NA",16,-62,false}},
        {"FM",{"Martinique",8,11,"NA",15,-61,false}},
        {"FJ",{"Saint Barthelemy",8,11,"NA",18,-63,false}},
        {"VP5",{"Turks & Caicos Is.",8,11,"NA",21,-72,false}},
        {"VP9",{"Bermuda",5,11,"NA",32,-65,false}},
        {"ZF",{"Cayman Is.",8,11,"NA",19,-81,false}},
        {"KG4",{"Guantanamo Bay",8,11,"NA",20,-75,false}},
        {"KP1",{"Navassa Is.",8,11,"NA",18,-75,false}},
        {"KP2",{"US Virgin Is.",8,11,"NA",18,-65,false}},
        {"KP4",{"Puerto Rico",8,11,"NA",18,-67,false}},
        {"KP5",{"Desecheo Is.",8,11,"NA",18,-68,false}},
        {"FP",{"St. Pierre & Miquelon",5,9,"NA",47,-56,false}},
        {"VO1|VO2",{"Canada",2,9,"NA",60,-95,false}},
        {"VY1|VY0",{"Canada",1,1,"NA",60,-95,false}},
        {"KL|AL|NL|WL",{"Alaska",1,1,"NA",61,-150,false}},
        {"KH6|AH6|NH6|WH6",{"Hawaii",31,61,"OC",20,-156,false}},
        {"KH8",{"American Samoa",32,62,"OC",-14,-170,false}},
        {"KH0",{"Mariana Is.",27,64,"OC",15,146,false}},
        {"KH2",{"Guam",27,64,"OC",13,145,false}},
        {"KH3",{"Johnston Is.",31,61,"OC",17,-170,false}},
        {"KH4",{"Midway Is.",31,61,"OC",28,-177,false}},
        {"KH5",{"Palmyra & Jarvis Is.",31,61,"OC",6,-162,false}},
        {"KH9",{"Wake Is.",31,65,"OC",19,167,false}},
        {"VQ9",{"Chagos Is.",39,41,"AF",-6,72,false}},

        // Caribbean
        {"J3",{"Grenada",8,11,"NA",12,-62,false}},
        {"J6",{"St. Lucia",8,11,"NA",14,-61,false}},
        {"J7",{"Dominica",8,11,"NA",15,-61,false}},
        {"J8",{"St. Vincent",8,11,"NA",13,-61,false}},
        {"V2",{"Antigua & Barbuda",8,11,"NA",17,-62,false}},
        {"V4",{"St. Kitts & Nevis",8,11,"NA",17,-63,false}},
        {"8P",{"Barbados",8,11,"NA",13,-59,false}},
        {"9Y",{"Trinidad & Tobago",9,11,"SA",11,-61,false}},
        {"CO|T4|CL|CM|CM",{"Cuba",8,11,"NA",22,-80,false}},
        {"HH",{"Haiti",8,11,"NA",19,-72,false}},
        {"HI",{"Dominican Republic",8,11,"NA",19,-71,false}},

        // South America
        {"LU|AY|LO",{"Argentina",13,14,"SA",-38,-65,false}},
        {"PY|PP|PQ|PT|PR|PU|ZV-ZZ",{"Brazil",11,15,"SA",-8,-51,false}},
        {"CE|CA|CB|CD|3G",{"Chile",12,14,"SA",-30,-71,false}},
        {"HK|5J|5K",{"Colombia",9,12,"SA",4,-72,false}},
        {"HC|HD",{"Ecuador",10,12,"SA",-2,-77,false}},
        {"OA|OB|OC",{"Peru",10,12,"SA",-10,-76,false}},
        {"YV|4M",{"Venezuela",9,12,"SA",8,-66,false}},
        {"CP",{"Bolivia",10,12,"SA",-17,-64,false}},
        {"PY0F",{"Fernando de Noronha",11,15,"SA",-4,-33,false}},
        {"PY0S",{"St. Peter & St. Paul Rocks",11,15,"SA",1,-29,false}},
        {"PY0T",{"Trindade & Martim Vaz",11,15,"SA",-20,-29,false}},
        {"CE0Y",{"Easter Is.",12,63,"SA",-27,-109,false}},
        {"CE0X",{"San Felix",12,14,"SA",-26,-80,false}},
        {"GY",{"Guyana",9,12,"SA",5,-59,false}},
        {"PZ",{"Suriname",9,12,"SA",4,-56,false}},
        {"FY",{"French Guiana",9,12,"SA",4,-53,false}},
        {"CX",{"Uruguay",13,14,"SA",-33,-56,false}},
        {"ZP",{"Paraguay",11,14,"SA",-23,-58,false}},
        {"ZL",{"New Zealand",32,60,"OC",-41,175,false}},
        {"VK",{"Australia",29,55,"OC",-25,134,false}},

        // Europe
        {"EA|EB|EC|ED|EE|EF|EG|EH|AM-AO",{"Spain",14,21,"EU",40,-4,false}},
        {"F|TM",{"France",14,27,"EU",46,2,false}},
        {"DL|DA-DR",{"Germany",14,28,"EU",51,10,false}},
        {"I|IA-IZ",{"Italy",15,28,"EU",42,12,false}},
        {"ON|OO-OT",{"Belgium",14,27,"EU",51,4,false}},
        {"PA|PB|PC|PD|PE|PF|PG|PH|PI",{"Netherlands",14,27,"EU",52,5,false}},
        {"LX",{"Luxembourg",14,27,"EU",50,6,false}},
        {"HB|HE",{"Switzerland",14,28,"EU",47,8,false}},
        {"OE",{"Austria",15,28,"EU",47,14,false}},
        {"G|GX|M",{"England",14,27,"EU",52,-2,false}},
        {"GD",{"Isle of Man",14,27,"EU",54,-4,false}},
        {"GI|MI",{"Northern Ireland",14,27,"EU",55,-6,false}},
        {"GJ|MJ",{"Jersey",14,27,"EU",49,-2,false}},
        {"GM|MM",{"Scotland",14,27,"EU",56,-4,false}},
        {"GU|MU",{"Guernsey",14,27,"EU",49,-3,false}},
        {"GW|MW",{"Wales",14,27,"EU",52,-4,false}},
        {"EI|EJ",{"Ireland",14,27,"EU",53,-8,false}},
        {"SM|SA-SK",{"Sweden",18,18,"EU",60,15,false}},
        {"LA|LB|LC|LG|LJ|LN",{"Norway",18,18,"EU",60,10,false}},
        {"OZ",{"Denmark",18,18,"EU",56,10,false}},
        {"OH",{"Finland",18,18,"EU",60,25,false}},
        {"OH0",{"Aland Is.",18,18,"EU",60,20,false}},
        {"OJ0",{"Market Reef",18,18,"EU",60,19,false}},
        {"TF",{"Iceland",40,17,"EU",65,-18,false}},
        {"JW",{"Svalbard",40,18,"EU",78,20,false}},
        {"JX",{"Jan Mayen",40,18,"EU",71,-8,false}},
        {"OX",{"Greenland",40,5,"NA",72,-40,false}},
        {"SP|SN-SR|3Z",{"Poland",15,28,"EU",52,20,false}},
        {"OK|OL",{"Czech Republic",15,28,"EU",50,15,false}},
        {"OM",{"Slovak Republic",15,28,"EU",48,19,false}},
        {"HG|HA",{"Hungary",15,28,"EU",47,19,false}},
        {"YO|YP-YR",{"Romania",20,28,"EU",46,25,false}},
        {"LZ",{"Bulgaria",20,28,"EU",43,25,false}},
        {"SV|J4",{"Greece",20,28,"EU",38,24,false}},
        {"SV9",{"Crete",20,28,"EU",35,25,false}},
        {"SV5|J45",{"Dodecanese",20,28,"EU",36,28,false}},
        {"SV/A",{"Mount Athos",20,28,"EU",40,24,false}},
        {"YU|4N",{"Serbia",15,28,"EU",44,21,false}},
        {"9A",{"Croatia",15,28,"EU",45,16,false}},
        {"S5",{"Slovenia",15,28,"EU",46,15,false}},
        {"E7",{"Bosnia-Herzegovina",15,28,"EU",44,17,false}},
        {"T9",{"Bosnia-Herzegovina",15,28,"EU",44,17,false}},
        {"Z3",{"North Macedonia",15,28,"EU",42,22,false}},
        {"4O",{"Montenegro",15,28,"EU",43,19,false}},
        {"Z6",{"Kosovo",15,28,"EU",42,21,false}},
        {"ZA",{"Albania",15,28,"EU",41,20,false}},
        {"4U1I",{"ITU HQ",14,28,"EU",46,7,false}},
        {"4U1V",{"Vienna Intl Centre",15,28,"EU",48,16,false}},
        {"HV",{"Vatican",15,28,"EU",42,12,false}},
        {"T7",{"San Marino",15,28,"EU",44,12,false}},
        {"3A",{"Monaco",14,27,"EU",44,7,false}},
        {"ES",{"Estonia",15,29,"EU",59,25,false}},
        {"LY",{"Lithuania",15,29,"EU",56,24,false}},
        {"YL",{"Latvia",15,29,"EU",57,25,false}},
        {"EU|EV|EW",{"Belarus",16,29,"EU",53,28,false}},
        {"ER",{"Moldova",16,29,"EU",47,29,false}},
        {"UB|UC|UD|UF|UG|UH|UI|UK|UR|US|UT|UV|UX|UY|UZ|EM|EN|EO|UR",{"Ukraine",16,29,"EU",49,31,false}},
        {"RA|RB|RC|RD|RE|RF|RG|RH|RI|RJ|RK|RL|RM|RN|RO|RP|RQ|RS|RT|RU|RV|RW|RX|RY|RZ|UA|UB|UC|UF",
            {"Russia",16,30,"EU",60,80,false}},
        {"UR|US|UT|UV|UX|UY|UZ",{"Ukraine",16,29,"EU",49,31,false}},

        // Specific Russian regions
        {"UA2",{"Kaliningrad",15,29,"EU",55,21,false}},
        {"UA0",{"Russia Asia",23,30,"AS",60,100,false}},

        // Africa
        {"ZS|ZT|ZU",{"South Africa",38,57,"AF",-29,25,false}},
        {"ZD8",{"Ascension Is.",36,66,"AF",-8,-14,false}},
        {"ZD9",{"Tristan da Cunha",38,66,"AF",-37,-12,false}},
        {"7P",{"Lesotho",38,57,"AF",-29,28,false}},
        {"3DA",{"Swaziland (eSwatini)",38,57,"AF",-26,31,false}},
        {"A2",{"Botswana",38,57,"AF",-22,24,false}},
        {"V5",{"Namibia",38,57,"AF",-22,17,false}},
        {"Z2",{"Zimbabwe",38,53,"AF",-20,30,false}},
        {"C9",{"Mozambique",37,53,"AF",-18,35,false}},
        {"ZL0",{"Auckland & Campbell",32,60,"OC",-52,170,false}},

        // Middle East / Asia
        {"JA|7J|8J",{"Japan",25,45,"AS",36,138,false}},
        {"HL|6K-6N|DS-DT",{"South Korea",25,44,"AS",37,128,false}},
        {"BY|BV|BW|BX|BA-BZ|3W|XW",{"China",24,44,"AS",36,104,false}},
        {"VU|AT|AU|AV|AW",{"India",26,41,"AS",22,80,false}},
        {"9N",{"Nepal",26,42,"AS",28,84,false}},
        {"S2|S3",{"Bangladesh",26,41,"AS",24,90,false}},
        {"4S",{"Sri Lanka",26,41,"AS",7,81,false}},
        {"AP|AS",{"Pakistan",21,41,"AS",30,70,false}},
        {"TA|TC|YM",{"Turkey",20,39,"AS",39,35,false}},
        {"4X|4Z",{"Israel",20,39,"AS",32,35,false}},
        {"YK",{"Syria",20,39,"AS",35,38,false}},
        {"OD",{"Lebanon",20,39,"AS",34,36,false}},
        {"JY",{"Jordan",20,39,"AS",32,36,false}},
        {"A4",{"Oman",21,39,"AS",22,58,false}},
        {"A7",{"Qatar",21,39,"AS",25,51,false}},
        {"A9",{"Bahrain",21,39,"AS",26,51,false}},
        {"A6",{"UAE",21,39,"AS",24,54,false}},
        {"HZ",{"Saudi Arabia",21,39,"AS",25,45,false}},
        {"7Z",{"Saudi Arabia",21,39,"AS",25,45,false}},
        {"YI",{"Iraq",21,39,"AS",33,44,false}},
        {"EP|EQ",{"Iran",21,40,"AS",32,54,false}},
        {"EK",{"Armenia",21,29,"AS",40,45,false}},
        {"4J|4K",{"Azerbaijan",21,29,"AS",40,48,false}},
        {"4L",{"Georgia",21,29,"AS",42,44,false}},
        {"UN|UO",{"Kazakhstan",17,30,"AS",48,68,false}},
        {"EX",{"Kyrgyzstan",17,30,"AS",41,75,false}},
        {"EY",{"Tajikistan",17,30,"AS",39,71,false}},
        {"EZ",{"Turkmenistan",17,30,"AS",40,59,false}},
        {"UK",{"Uzbekistan",17,30,"AS",41,64,false}},
        {"TN",{"Rep of Congo",36,52,"AF",-1,15,false}},
        {"9Q|9T",{"Dem. Rep. Congo",36,52,"AF",-4,22,false}},
        {"TL",{"Central Africa",36,52,"AF",7,21,false}},
        {"TT",{"Chad",35,47,"AF",15,19,false}},
        {"XU",{"Cambodia",26,49,"AS",12,105,false}},
        {"XV|3W",{"Vietnam",26,49,"AS",16,108,false}},
        {"XZ",{"Myanmar",26,49,"AS",17,96,false}},
        {"HS|E2",{"Thailand",26,49,"AS",15,102,false}},
        {"XW",{"Laos",26,49,"AS",18,103,false}},
        {"VS6|VR",{"Hong Kong",24,44,"AS",22,114,false}},
        {"XX9",{"Macao",24,44,"AS",22,113,false}},
        {"9V",{"Singapore",28,54,"OC",1,104,false}},
        {"YB|YC|YD|YE|YF|YG|YH",{"Indonesia",28,54,"OC",-8,120,false}},
        {"DU|4D|4E|4F|4G|4I|DV",{"Philippines",27,50,"OC",12,122,false}},
        {"VK9L",{"Lord Howe Is.",30,60,"OC",-32,159,false}},
        {"VK9N",{"Norfolk Is.",32,60,"OC",-29,168,false}},
        {"VK9X",{"Christmas Is.",29,54,"OC",-11,105,false}},
        {"VK9C",{"Cocos-Keeling Is.",29,54,"OC",-12,97,false}},
        {"9M2|9M4",{"West Malaysia",26,54,"OC",4,102,false}},
        {"9M6|9M8",{"East Malaysia",28,54,"OC",4,114,false}},
        {"T8",{"Palau",27,64,"OC",7,134,false}},
        {"V6",{"Micronesia",27,65,"OC",7,152,false}},
        {"KH1",{"Baker & Howland Is.",31,61,"OC",0,-176,false}},

        // Pacific
        {"ZK3",{"Tokelau Is.",31,62,"OC",-9,-172,false}},
        {"A35",{"Tonga",32,62,"OC",-20,-175,false}},
        {"3D2",{"Fiji",28,56,"OC",-18,178,false}},
        {"YJ",{"Vanuatu",28,56,"OC",-16,168,false}},
        {"H4",{"Solomon Is.",28,51,"OC",-9,160,false}},
        {"P2",{"Papua New Guinea",28,51,"OC",-6,147,false}},
        {"ZL7",{"Chatham Is.",32,60,"OC",-44,-177,false}},
        {"ZL8",{"Kermadec Is.",32,60,"OC",-29,-178,false}},
        {"ZL9",{"Sub-Antarctic Is.",32,60,"OC",-51,169,false}},
        {"VQ9",{"Chagos Is.",39,41,"AF",-7,72,false}},
        {"CE9|KC4|LU|OR4",{"Antarctica",38,67,"AN",-90,0,false}},
        {"VP8",{"Falkland Is.",13,16,"SA",-52,-59,false}},

        // Africa continued
        {"9X",{"Rwanda",36,52,"AF",-2,30,false}},
        {"9U",{"Burundi",36,52,"AF",-3,30,false}},
        {"5H",{"Tanzania",37,53,"AF",-6,35,false}},
        {"5I",{"Tanzania",37,53,"AF",-6,35,false}},
        {"5Z",{"Kenya",37,53,"AF",0,37,false}},
        {"7Q",{"Malawi",37,53,"AF",-13,34,false}},
        {"Z2",{"Zimbabwe",38,53,"AF",-20,30,false}},
        {"3B9",{"Rodrigues Is.",39,53,"AF",-20,63,false}},
        {"3B8",{"Mauritius",39,53,"AF",-20,57,false}},
        {"3B6|3B7",{"Agalega & St. Brandon Is.",39,53,"AF",-11,57,false}},
        {"5R",{"Madagascar",39,53,"AF",-18,47,false}},
        {"FR",{"Reunion Is.",39,53,"AF",-21,56,false}},
        {"FT5W",{"Crozet Is.",38,68,"AF",-46,52,false}},
        {"FT5X",{"Kerguelen Is.",39,68,"AF",-49,70,false}},
        {"FT5Z",{"Amsterdam & St. Paul Is.",39,68,"AF",-37,77,false}},

        // More Africa
        {"SU",{"Egypt",34,38,"AF",27,30,false}},
        {"ST",{"Sudan",34,48,"AF",13,32,false}},
        {"SS",{"Sudan",34,48,"AF",13,32,false}},
        {"5T",{"Mauritania",35,46,"AF",20,-11,false}},
        {"6W",{"Senegal",35,46,"AF",15,-14,false}},
        {"EL",{"Liberia",35,46,"AF",6,-10,false}},
        {"9G",{"Ghana",35,46,"AF",8,-1,false}},
        {"TY",{"Benin",35,46,"AF",10,2,false}},
        {"XT",{"Burkina Faso",35,46,"AF",12,-2,false}},
        {"TZ",{"Mali",35,46,"AF",17,-4,false}},
        {"6O",{"Somalia",37,48,"AF",6,46,false}},
        {"T5",{"Somalia",37,48,"AF",6,46,false}},
        {"5A",{"Libya",33,38,"AF",28,17,false}},
        {"CN",{"Morocco",33,37,"AF",32,-6,false}},
        {"7X",{"Algeria",33,37,"AF",28,2,false}},
        {"TS",{"Tunisia",33,37,"AF",34,9,false}},
        {"ET",{"Ethiopia",37,48,"AF",9,39,false}},
        {"J2",{"Djibouti",37,48,"AF",12,43,false}},
        {"T3",{"Western Kiribati",31,65,"OC",0,174,false}},
        {"T30",{"Western Kiribati",31,65,"OC",0,174,false}},
        {"T31",{"Central Kiribati",31,62,"OC",-3,-172,false}},
        {"T32",{"Eastern Kiribati",31,61,"OC",2,-157,false}},
        {"T33",{"Banaba Is.",28,65,"OC",-1,170,false}},

        // Sentinel
        {nullptr,{}}
    };

    for (const Row* r = table; r->prefixes; ++r) {
        QString raw = QString::fromLatin1(r->prefixes);
        QStringList parts = raw.split('|');
        for (const QString& p : parts) {
            // Expand ranges like AA-AK → "AA","AB",...,"AK"
            // Format: p[0]=base, p[1]=range_start, p[2]='-', p[3]=base(ignored), p[4]=range_end
            if (p.size() == 5 && p[2] == '-') {
                QString base = p.left(1);         // e.g. "A"
                char c1 = p[1].toLatin1();        // e.g. 'A'
                char c2 = p[4].toLatin1();        // e.g. 'K'
                for (char c = c1; c <= c2; ++c)
                    s_map[base + QChar(c)] = r->ent;
            } else {
                s_map[p] = r->ent;
            }
        }
    }

    s_keys = s_map.keys();
    std::sort(s_keys.begin(), s_keys.end(),
              [](const QString& a, const QString& b){ return a.size() > b.size(); });
}

// ---------------------------------------------------------------------------

void init()
{
    QMutexLocker lk(&s_mutex);
    if (!s_map.isEmpty()) return;
    buildTable();
}

DxccInfo lookup(const QString& callsign)
{
    QMutexLocker lk(&s_mutex);
    if (s_map.isEmpty()) {
        lk.unlock();
        init();
        lk.relock();
    }

    // Normalise
    QString cs = callsign.toUpper().trimmed();

    // Strip /P /M /MM /AM suffixes but keep /country-prefix appended at end
    // e.g. IW8XOU/P -> IW8XOU, IW8XOU/F -> treat F as the prefix
    QString effective = cs;
    int slash = cs.lastIndexOf('/');
    if (slash > 0) {
        QString prefix = cs.left(slash);
        QString suffix = cs.mid(slash+1);
        if (suffix == "P" || suffix == "M" || suffix == "MM" ||
            suffix == "AM" || suffix == "QRP")
            effective = prefix;
        else if (suffix.size() <= 3 && !suffix[0].isDigit())
            effective = suffix;  // /F, /VK, etc.
    }

    // Greedy longest-prefix match
    for (const QString& key : s_keys) {
        if (effective.startsWith(key)) {
            const Entity& e = s_map[key];
            return DxccInfo{e.name, e.cqZone, e.ituZone, e.continent, e.lat, e.lon};
        }
    }

    return DxccInfo{};
}

QStringList allEntities()
{
    if (s_map.isEmpty()) init();
    QStringList names;
    QSet<QString> seen;
    for (const Entity& e : s_map.values()) {
        if (!seen.contains(e.name)) {
            seen.insert(e.name);
            names.append(e.name);
        }
    }
    names.sort();
    return names;
}

int entityCount()
{
    if (s_map.isEmpty()) init();
    QSet<QString> names;
    for (const Entity& e : s_map.values())
        names.insert(e.name);
    return names.size();
}

} // namespace Dxcc
