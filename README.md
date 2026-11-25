# VSA-LAB4TEST

## Məqsəd və iş sahəsi

Bu, VSA1 (Verilənlərin Strukturu və Alqoritmlər–1) laboratoriya işinin yalnız
**Task 1 — Separate Chaining və MAD Compression** hissəsidir. Universitet kitabxanasının
kitablarını ISBN açarı ilə saxlayır. C++17 və standart kitabxanadan istifadə olunur;
hazır hash table, `std::unordered_map`, verilənlər bazası və xarici test framework-u yoxdur.

İstifadəçinin təsdiqlədiyi iş qovluğu:
`C:\Users\nihad\githubrepocopies\VSA-LAB4TEST`.
Təlimatdakı LAB4-TEST bu mövcud qovluq kimi qəbul edilib; əlavə layihə qovluğu yaradılmayıb.
Git əməliyyatları aparılmayıb. Mövcud `.git/` qovluğu dəyişdirilməyib.

## Fayl quruluşu

```text
VSA-LAB4TEST/
├── include/
│   ├── Book.h
│   ├── HashUtils.h
│   └── SeparateChainingHash.h
├── src/
│   ├── HashUtils.cpp
│   └── SeparateChainingHash.cpp
├── tests/
│   └── test_chaining.cpp
├── build/                     # Bütün build, test və portativ alət artefaktları
├── main.cpp
├── CMakeLists.txt
├── .gitignore
└── README.md
```

`include/` müqaviləni (declarations), `src/` isə reallaşdırmanı (definitions) saxlayır.
`main.cpp` və testlər eyni `chaining` kitabxanasından istifadə edir.
Başlıq fayllarındakı `#pragma once` eyni başlığın bir translation unit daxilində təkrar
daxil edilməsini dayandırır. `#include "…"` yerli başlıq, `#include <…>` standart başlıq üçündür.

## Tələb olunan alətlər və yığma

C++17 kompilyatoru və CMake 3.16+ lazımdır. Bu kompüterdə MSVC 14.51.36231
(Visual Studio 18 Community) aşkar edilib. Əlavə C++ kitabxanası lazım deyil.
CMake sistem PATH-ində olmadığı üçün istifadəçinin icazəsi ilə portativ paket
`build/tools/` daxilində hazırlanıb.

CMake PATH-də olduqda, terminalı layihənin kökündə açın:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\test_chaining.exe
.\build\Debug\library_catalogue.exe

cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\library_catalogue.exe
```

`-A x64` Visual Studio generatoru üçündür. GCC/Clang və tək konfiqurasiyalı
generator üçün `-A x64` yazmayın; `-DCMAKE_BUILD_TYPE=Debug` və ya `Release`
verin. Ayrı konfiqurasiyalar üçün `build/gcc-debug/`, `build/gcc-release/` kimi
yalnız `build/` altındakı qovluqlardan istifadə edin. Bu halda executable
konfiqurasiya altqovluğu olmadan müvafiq build qovluğunda yerləşir.

`CTest` bir test executable-ı başladır; həmin executable daxilində 36 ayrıca
test halı var. Ona görə CTest-in “1/1” nəticəsi daxili 36 testi ifadə edir.
Hər testin adını görmək üçün `test_chaining.exe`-ni birbaşa işlədin.

Sanitizer komponentləri quraşdırılmış mühitdə ayrıca yoxlama:

```powershell
cmake -S . -B build/asan -A x64 -DENABLE_SANITIZERS=ON
cmake --build build/asan --config RelWithDebInfo
ctest --test-dir build/asan -C RelWithDebInfo --output-on-failure
```

MSVC-də `RelWithDebInfo` istifadə edilir: standart Debug-un `/RTC1` seçimi
AddressSanitizer ilə uyğun deyil. GCC/Clang üçün `-A x64` əvəzinə
`-DCMAKE_BUILD_TYPE=Debug` yazın. MSVC yalnız AddressSanitizer, GCC/Clang isə
AddressSanitizer və UndefinedBehaviorSanitizer bayraqlarını alır.

## 1. Book.h — saxlanılan məlumat

`<string>` kitabxanası `std::string` üçün lazımdır. `struct Book` dörd public sahə saxlayır:

| Sahə | Tip | Mənası |
|---|---|---|
| `isbn` | `std::string` | Unikal açar; bütün əməliyyatlar bununla aparılır |
| `title` | `std::string` | Kitabın adı |
| `author` | `std::string` | Müəllif |
| `isAvailable` | `bool` | Kitabın götürülmək üçün mövcud olub-olmaması |

ISBN hesablanan ədəd deyil, identifikatordur. `"001"` ilə `"1"` fərqli açarlardır.
String başlanğıc sıfırlarını, tireləri və ISBN-10-dakı `X` simvolunu qoruyur.
Açarlar olduğu kimi müqayisə olunur: format təmizlənməsi, checksum və real ISBN
validasiyası bu tapşırığa daxil deyil. Boş string də texniki olaraq etibarlı, unikal açardır.

`Book{isbn, title, author, available}` aggregate initialization-dır: sahələr
elan sırası ilə başlanğıc qiymət alır. Kitab sinfə daxil ediləndə kopyalanır.
Sonradan çağıranın öz `Book` obyektini dəyişməsi saxlanmış kitabı dəyişmir.

## 2. HashUtils.h və HashUtils.cpp — ümumi hashing modulu

Başlıq `<cstddef>`-dən `std::size_t`, `<cstdint>`-dən dəqiq enli unsigned tiplər,
`<string>`-dən açar tipini alır. İcra faylı `<limits>` ilə tip sərhədlərini,
`<stdexcept>` ilə istisnaları istifadə edir. `namespace HashUtils` adları
digər modulların eyniadlı funksiyalarından ayırır.

### polynomialHash(key, base = 37)

Horner qaydasına əsaslanan polynomial hash belədir:

```cpp
std::uint64_t hash = 0;
for (unsigned char character : key) {
    hash = hash * base + character;
}
return hash;
```

Sətirlərin mənası:

1. `hash = 0` başlanğıc yığıcıdır; boş string üçün dövr işləmir və sıfır qayıdır.
2. Range-based `for` string-in hər baytını sıra ilə oxuyur.
3. `unsigned char` yüksək biti olan baytların mənfi ədəd sayılmasının qarşısını alır.
4. Əvvəlki nəticə baza ilə vurulur, sonra cari bayt əlavə edilir.
5. Son yığıcı 64 bitlik hash kodu kimi qaytarılır.

`"AB"` üçün ASCII qiymətləri 65 və 66-dır:

```text
başlanğıc:       0
A-dan sonra:    0 × 37 + 65 = 65
B-dən sonra:   65 × 37 + 66 = 2471
```

Üç simvol üçün nəticə `c0 × base² + c1 × base + c2`-yə uyğundur.
Horner qaydası dərəcələri ayrıca hesablamağa ehtiyac qoymur; bir keçiddə O(L) vaxt,
O(1) əlavə yaddaş sərf edir. L açarın bayt sayıdır.

37 kiçik sadə ədəddir və simvolların sırasını nəzərə alan praktik başlanğıc seçimidir;
bütün məlumatlar üçün optimal və ya kolliziyasız olması iddia edilmir.
Baza 1-də `"AB"` və `"BA"` eyni 131 nəticəsini verir; anaqramlar həmişə toqquşur.
Baza 37-də `"AB" = 2471`, `"BA" = 2507` olur.
Baza konfiqurasiya edilə bilir; baza 0 da rədd edilmir və boş olmayan açarda son baytı saxlayır.

`std::uint64_t` daşdıqda nəticə C++ qaydalarına görə modulo 2^64 alınır.
Bu, signed integer overflow kimi undefined behavior deyil. Ancaq məlumat itirdiyinə
görə fərqli açarlar eyni hash ala bilər. UTF-8 string baytlar üzrə işlənir, Unicode
simvolları və ya mətn normallaşdırılması üzrə deyil.

### moduloCompress(hash, capacity)

İlk `if` sıfır tutumu `std::invalid_argument` ilə rədd edir; beləliklə sıfıra
modulo əməliyyatı baş vermir. Sonra `hash % capacity` qaytarılır.
`static_cast<std::size_t>` nəticəni indeks tipinə çevirir.
Müsbət N üçün qalıq həmişə `0 <= index < N` aralığındadır.

Məsələn, `131 % 11 = 10`. Funksiya O(1) vaxt və O(1) yaddaş istifadə edir.

### MAD parametrləri və daşmanın sübutu

Ümumi düstur:

```text
h(k) = ((a × (k mod p) + b) mod p) mod N
```

Qaydalar: p sadədir, p > N, 1 <= a < p, 0 <= b < p.
Bu implementasiyada dəstəklənən əlavə sərhəd `p <= UINT32_MAX = 2^32 − 1`-dir.
k isə istənilən `uint64_t` qiyməti ola bilər.

Azaldılmış hash ən çox p−1, a və b də ən çox p−1 olduğundan:

```text
a × (k % p) + b <= (p−1)² + (p−1)
                 = p × (p−1)
                 < 2^64
```

Deməli vurma və toplama `uint64_t` daxilində dəqiqdir.
32 bitdən böyük p açıq şəkildə rədd edilir; səssiz daşma və compiler-specific
`__int128` istifadəsi yoxdur. Bu sərhəd sayəsində ayrıca modular multiplication
alqoritminə ehtiyac qalmır.

Cədvəlin təkrarolunan parametrləri:

- `defaultMadPrime = 4294967291`: 32 bit aralığında sadə ədəd.
- `defaultMadA = 1664525`: MAD vurucusu.
- `defaultMadB = 1013904223`: əlavə edilən sürüşmə.

`inline constexpr` bunları C++17-də başlıqda təhlükəsiz sabit kimi elan edir.
Mutable qlobal vəziyyət və hər insert üçün təsadüfi parametr seçilməsi yoxdur.

**Polynomial base və MAD multiplier fərqlidir.** Base string-dən k yaradır;
MAD-in a parametri hazır k-nı indeksə çevirməyin ikinci mərhələsində işləyir.
MAD, artıq eyni polynomial hash almış iki açarı bir-birindən ayıra bilməz.

### MadCompressor sinfi

Bu əlavə helper sinif p-nin sadəliyini hər axtarışda yenidən yoxlamamaq üçündür.

- `a_`: yoxlanmış vurucu.
- `b_`: yoxlanmış əlavə.
- `p_`: yoxlanmış sadə modul.
- Üçü də private-dır; istifadəçi yoxlamadan sonra onları dəyişə bilmir.

`MadCompressor(a, b, p)` konstruktorunun icra sırası:

1. Member initializer list qiymətləri sahələrə yazır.
2. `p < 2` və ya `p > UINT32_MAX` olduqda istisna atılır.
3. `||` short-circuit olduğu üçün böyük p-ni daraldıb sadəlik testinə ötürmür.
4. Sərhəd daxilində p `isPrime` ilə yoxlanır.
5. a və b-nin aralıqları yoxlanır; uyğunsuz parametr yenə istisnadır.

`compress(hash, capacity) const`:

1. N sıfırdırsa və ya N >= p-dirsə istisna atır.
2. `reduced = hash % p_` böyük hash-i təhlükəsiz aralığa salır.
3. `a_ * reduced + b_` yuxarıdakı sübuta görə daşmır.
4. Birinci `% p_` affine çevirməni sonlu modul daxilində tamamlayır.
5. İkinci `% capacity` nəticəni bucket indeksinə çevirir.
6. `const` metod obyektin parametrlərini dəyişmir.

Məsələn, k=100, a=3, b=7, p=101, N=11:
`100 % 101 = 100`, `3×100+7=307`, `307%101=4`, `4%11=4`.

`madCompress(hash, capacity, a, b, p)` tələb olunan sərbəst funksiyadır:
müvəqqəti `MadCompressor` yaradıb `compress` çağırır.
O, hər çağırışda parametrləri yenidən yoxlayır. Təkrarlanan işdə sinfi bir dəfə
yaratmaq daha sərfəlidir; hash table məhz belə edir.

### isPrime(number)

1. 0 və 1 üçün false qaytarır.
2. Cüt ədədlər içində yalnız 2 üçün true qaytarır.
3. 3-dən başlayaraq yalnız tək bölənləri sınayır.
4. `divisor <= number / divisor` şərti `divisor² <= number` deməkdir,
   lakin vurma daşması yaratmır.
5. Bölünmə tapılarsa false; heç biri tapılmasa true qaytarılır.

Mürəkkəb ədədin ən azı bir böləni kvadrat kökdən böyük deyil; buna görə dövr
sqrt(number)-dən sonra davam etmir. Vaxt O(sqrt(number)), əlavə yaddaş O(1)-dir.

### nextPrime(number)

Bu funksiya verilən ədəddən **ciddi böyük** ilk sadə ədədi qaytarır.

1. `number == size_t::max()` halında daha böyük təsvir edilən ədəd yoxdur:
   `overflow_error` atılır.
2. Namizəd number+1 olur; 2 və aşağı qiymətlər üçün nəticə 2-dir.
3. Namizəd cütdürsə növbəti tək ədədə keçilir.
4. Sadəlik tapılanadək namizəd 2 artırılır.
5. Artımdan əvvəl maksimum sərhəd yoxlanır; wraparound-a yol verilmir.
6. İlk sadə namizəd qaytarılır.

`nextPrime(11)=13`, `nextPrime(22)=23`.
G namizəd yoxlanarsa təxminən O(G × sqrt(M)) vaxt sərf olunur; M namizədlərin ölçüsüdür.
Ən böyük size_t yaxınlığında prime axtarışının sürətli olmasına zəmanət verilmir.

### shrinkPrimeCapacity(currentCapacity, minimumCapacity)

Bu əlavə funksiya yarım tutuma ən yaxın sadə ədədi seçir.

1. Minimum sadə deyilsə və ya current minimumdan kiçikdirsə istisna atır.
2. `half = currentCapacity / 2` floor(N/2)-dir.
3. Half minimumdan böyük deyilsə minimum qaytarılır.
4. `lower` half-dan başlayıb aşağıdakı ilk sadə ədədə qədər azalır.
   Minimum sadə olduğundan dövr ondan aşağı düşmür.
5. `upperStart = half + currentCapacity % 2` ceil(N/2)-dir.
6. UpperStart sadədirsə özü, deyilsə növbəti sadə ədəd upper olur.
7. Aşağı namizədin ikiqat məsafəsi `N − lower − lower` ilə hesablanır.
8. Yuxarı namizədin ikiqat məsafəsi `upper − (N − upper)` ilə hesablanır.
   Bu yazılış `2 * upper` daşmasını və floating-point yuvarlaqlaşdırmasını aradan qaldırır.
9. `<=` bərabərlikdə aşağı sadə ədədi seçir.

23/2=11.5 üçün 11 seçilir; 17/2=8.5 üçün 7 seçilir;
12/2=6 üçün 5 və 7 bərabər məsafədədir, nəticə 5-dir.
Minimum 7 olduqda 11-in kiçilməsi 5-ə deyil, 7-yə aparılır.
Yaxın sadə ədədlərin axtarış xərci namizəd sayından asılıdır; yaddaş O(1)-dir.

## 3. SeparateChainingHash.h — cədvəlin müqaviləsi

Başlıq `Book.h`, `HashUtils.h`, ölçü/tam ədəd tipləri və `list`, `vector`,
`string` konteynerlərini daxil edir.

`enum class CompressionMode { MODULO, MAD }` tip təhlükəsiz seçimdir.
Dəyərlər `CompressionMode::MAD` kimi yazılır; adi integer ilə təsadüfən qarışmır.
Yanlış cast nəticəsi də konstruktorda ayrıca rədd edilir.

### Məlumat üzvləri

| Üzv | Məqsəd |
|---|---|
| `buckets_` | `vector<list<Book>>`: indekslənən bucket massivi və hər bucket-in zənciri |
| `minimumCapacity_` | Başlanğıcın normallaşdırılmış sadə tutumu; kiçilmə sərhədi |
| `mode_` | MODULO və ya MAD seçimi |
| `polynomialBase_` | String hashing bazası; adətən 37 |
| `autoResize_` | true: avtomatik böyümə/kiçilmə, false: sabit tutum |
| `elementCount_` | Saxlanmış unikal kitabların sayı n |
| `collisionCount_` | Uğurlu istifadəçi insert-lərinin yığılmış occupied-bucket collision sayı |
| `mad_` | Bir dəfə validasiya edilmiş sabit MAD parametrləri |

Vector bucket-ə O(1) indeksli çıxış verir. List hər toqquşan kitabı ayrıca node-da saxlayır.
`std::list` ikitərəfli əlaqəli siyahıdır; iterator tapıldıqdan sonra node silinməsi O(1)-dir.
Əlavə raw pointer zəncirləri yazmağa və manual `new/delete` istifadəsinə ehtiyac yoxdur.

### Konstruktor parametrləri

`explicit SeparateChainingHash(initialCapacity=11, mode=MODULO,
polynomialBase=37, autoResize=true)`:

- `initialCapacity`: müsbət olmalıdır. Sadədirsə saxlanır; deyilsə növbəti sadə ədədə yüksəlir.
  1 → 2, 10 → 11. Bu son qiymət minimum tutum olur.
- `mode`: iki sıxlaşdırma üsulundan birini seçir.
- `polynomialBase`: eyni hashing funksiyasına ötürülür; Task 3 üçün 1/37 müqayisəsinə hazırdır.
- `autoResize`: false olduqda tutum dəyişmir; λ hətta 1-dən böyük ola bilər.
- `explicit`: məsələn, tam ədəddən təsadüfi implicit cədvəl çevrilməsini qadağan edir.

`const Book&` və `const std::string&` mövcud obyektə read-only istinaddır:
parametr ötürülərkən lazımsız tam kopya yaradılmır.
`const Book*` saxlanmış kitaba read-only pointer-dir, mülkiyyət ötürülmür.
Header yalnız elanları saxlayır; metodların gövdələri .cpp-dədir.

## 4. SeparateChainingHash.cpp — əməliyyatların gedişi

`<algorithm>` maximum üçün, `<limits>` daşma sərhədləri üçün,
`<stdexcept>` səhv arqument və ölçü xətaları üçün lazımdır.

### normalizedCapacity(requested, mode)

Adı olmayan namespace daxilindəki bu helper yalnız həmin .cpp faylından görünür.

1. Sıfır başlanğıc tutumu rədd edir.
2. Naməlum compression mode dəyərini rədd edir.
3. MAD üçün requested >= sabit p olarsa böyük vector ayırmağa başlamadan rədd edir.
4. Requested sadədirsə olduğu kimi saxlayır; deyilsə `nextPrime` çağırır.
5. Normallaşdırmadan sonra da MAD sərhədini yoxlayır: sadə ədədə yüksəlmə p-yə çata bilər.
6. Nəticə vector-un başlanğıc ölçüsünə verilir.

Konstruktorun initializer list-i əvvəl `buckets_` yaradır, sonra onun ölçüsünü
`minimumCapacity_` edir, digər parametrləri və `mad_` obyektini qurur.
Üzvlərin real qurulma sırası başlıqda elan sırasıdır.
Sayğaclar başlıqdakı `= 0` başlanğıc qiymətlərini alır.

### bucketIndex(isbn, targetCapacity)

Bu private metod bütün əməliyyatlarda ortaq indeks hesablamasıdır:

1. `HashUtils::polynomialHash(isbn, polynomialBase_)` çağırılır.
2. `mode_ == MODULO` olduqda modulo, əks halda əvvəlcədən yoxlanmış MAD işlədilir.
3. Şərti `?:` operatoru yalnız seçilmiş qolu hesablayır.
4. TargetCapacity ayrıca parametrdir: rehash zamanı cari deyil, yeni N istifadə edilir.

Bu paylaşım insert, remove və find-in fərqli qaydalarla hash hesablamasının qarşısını alır.
Vaxt O(L), əlavə yaddaş O(1)-dir.

### insert(const Book& b) — addım-addım

İcra ardıcıllığı:

1. `index = bucketIndex(b.isbn, capacity())`: cari N ilə bucket seçilir.
2. `auto& chain = buckets_[index]`: list kopyalanmır, istinad alınır.
3. `for (const auto& stored : chain)`: yalnız həmin chain-dəki kitablar yoxlanır.
4. `stored.isbn == b.isbn` tapılarsa dərhal false qaytarılır.
   Mövcud title, author və isAvailable əvəz edilmir; say və statistika dəyişmir.
5. `collided = !chain.empty()`: həqiqi insert-dən əvvəl bucket dolu idimi?
6. Collision sayğacı maksimumda və yeni collision olacaqsa overflow_error atılır.
7. `chain.push_back(b)`: kitab list-in sonuna kopyalanır.
8. `++elementCount_`: unikal kitab sayı artırılır.
9. Auto-resize aktivdirsə `elementCount_ >= capacity()` şərti λ >= 1 deməkdir.
   Floating-point müqayisəsinə ehtiyac yoxdur.
10. `capacity() > max/2` yoxlaması `2 * capacity()` daşmasını qabaqlayır.
11. `rehash(nextPrime(2 * capacity()))` yeni bucket massivini qurur.
12. Rehash uğursuz olarsa `catch (...)` yeni kitabı `pop_back` ilə çıxarır,
    elementi azaldır və `throw;` ilə eyni istisnanı yuxarı ötürür.
    Rehash-in commit-dən əvvəl köhnə vector-u dəyişməməsi bu rollback-i mümkün edir.
13. Uğurlu insert üçün əvvəlki bucket dolu idisə collision sayğacı bir artırılır.
    Bu artım rehash-dən sonra yazılsa da ölçülən collision köhnə bucket-ə aiddir.
14. true qaytarılır.

Məsələn N=5, base=1, MODULO:
`"AB"` və `"BA"` üçün hash=131, indeks=1.
Birinci insert: bucket 1 `[AB]`, collision=0.
İkinci insert: bucket 1 `[AB, BA]`, collision=1.
Üçüncü dəfə `"AB"` daxil etmək false verir, n və collision dəyişmir.

Amortized expected insert xərci yaxşı paylanmada sabit uzunluqlu açarlar üçün O(1)-dir.
Tək insert bütün kitabları rehash edə bilər; buna görə hər insert-in worst-case O(1)
olduğu deyilmir. Dəqiq açar və rehash xərcləri aşağıdakı cədvəldə verilib.

### remove(const std::string& isbn)

1. Cari bucket hesablanır və həmin list-ə istinad götürülür.
2. `begin()`-dən `end()`-ə qədər iterator ilə gəzinilir.
3. `it->isbn` iteratorun göstərdiyi kitabın açarını oxuyur.
4. Uyğun kitab tapıldıqda `chain.erase(it)` node-u silir.
5. `--elementCount_` sayı azaldır.
6. Auto-resize aktiv, N minimumdan böyük və λ < 0.25-dirsə kiçilmə edilir.
7. Tam ədəd şərti `elementCount_ <= (capacity()-1)/4` tam olaraq `4*n < N`
   deməkdir; `4*n` vurmasını və mümkün daşmasını aradan qaldırır.
8. Yeni ölçü helper vasitəsilə seçilir və `rehash` çağırılır.
9. true qaytarılır. Dövr heç nə tapmadan bitərsə false qaytarılır.

Silinmiş iterator artıq istifadə edilmir. List-in orta node-unu silmək digər node-ları
yerindən tərpətmir. Ancaq bundan sonra rehash olarsa bütün əvvəlki book pointer-ləri etibarsızdır.

`[AB, BA, C@]` zəncirində `BA` silinsə `[AB, C@]` qalır.
Mövcud olmayan açarı silmək sayğacları, tutumu və collision tarixçəsini dəyişmir.

Yaddaş ayırma xətası kimi istisna shrink rehash-i zamanı baş verərsə silinmə artıq
tamamlanmış olur; köhnə tutum və qalan kitablar saxlanır. Bu əməliyyatın həmin
hal üçün təminatı basic exception guarantee-dir. Insert üçün rollback daha güclüdür.

### findByISBN(isbn) və findByISBN(isbn, probesOut)

Sadə overload lokal `ignored = 0` yaradıb probe sayan overload-u çağırır.
Beləliklə axtarış məntiqi təkrarlanmır.

Probe sayan overload:

1. `probesOut = 0` əvvəlki çağırışın nəticəsini sıfırlayır.
2. Cari bucket tapılır və read-only list istinadı alınır.
3. Hər saxlanmış Book üçün int sərhədi yoxlanır.
4. `++probesOut` yalnız bir ISBN müqayisəsi aparılacağına görə artırılır.
5. Açar bərabərdirsə `&book` qaytarılır: list node-undakı obyektin ünvanı.
6. Zəncir bitərsə `nullptr` qaytarılır.

`[AB, BA, C@]` zəncirində AB üçün 1, BA üçün 2, C@ üçün 3 probe var.
Eyni bucket-ə düşən, amma saxlanmayan D? üçün 3 probe;
boş bucket üçün 0 probe var. Hash hesablaması probe sayılmır.

Open Addressing-də bir probe boş və ya tombstone slotunun yoxlanması ola bilər.
Gələcək Task 3 müqayisəsində bu semantik fərq mütləq nəzərə alınmalıdır.

**Pointer lifetime:** qaytarılan pointer istifadəçinin siləcəyi obyekt deyil;
`delete` edilməməlidir. Kitab silindikdə, rehash zamanı, cədvəl məhv ediləndə
və ya onun məzmunu assignment ilə əvəzlənəndə pointer etibarsız olur.
Rehash etməyən list insert-i və başqa node-un silinməsi mövcud node ünvanını saxlayır,
amma praktik təhlükəsiz qayda mutation-dan sonra pointer-i yenidən axtarmaqdır.
Etibarsız pointer-i testlərdə dereference etmək özü undefined behavior olardı;
testlər rehash-dən sonra yeni pointer alır.

### rehash(newCapacity) — əsas keçid alqoritmi

```cpp
if (newCapacity == capacity()) return;
if (mode_ == CompressionMode::MAD && newCapacity >= HashUtils::defaultMadPrime)
    throw std::length_error("MAD capacity must remain smaller than p.");
std::vector<std::list<Book>> replacement(newCapacity);
for (const auto& chain : buckets_) {
    for (const auto& book : chain) {
        replacement[bucketIndex(book.isbn, newCapacity)].push_back(book);
    }
}
buckets_.swap(replacement);
```

Sətirlərin ardıcıllıqla mənası:

1. Eyni N üçün lazımsız işi buraxır.
2. MAD-in p > N şərtini qoruyur. p sabit olduğundan onu dəyişmək lazım gəlmir.
3. Yeni ölçüdə boş list-lərdən ibarət vector yaradır.
4. Xarici dövr köhnə bucket-ləri gəzir.
5. Daxili dövr hər bucket-dəki bütün kitabları gəzir.
6. Hər ISBN üçün yeni N ilə hash indeksi hesablanır.
7. `push_back(book)` bütün Book sahələrini yeni list-ə kopyalayır.
8. Bütün kopyalar uğurlu olduqdan sonra `swap` yeni vector-u aktiv edir.
9. Metod bitəndə replacement artıq köhnə vector-a sahibdir və onun destructor-u
   köhnə node-ları azad edir. Əvvəlki Book pointer-ləri burada etibarsızlaşır.

Public `insert` çağırılmır: duplicate axtarışı, avtomatik yeni resize və collision
artımı yoxdur. `elementCount_` də dəyişmir.
Kopyalama bitənədək xəta çıxsa müvəqqəti vector avtomatik təmizlənir,
köhnə cədvəl bütöv qalır (RAII və commit-after-copy yanaşması).

Köhnə indeksləri olduğu kimi saxlamaq səhvdir.
Məsələn 131 % 5 = 1, ancaq 131 % 11 = 10.
Yeni axtarış bucket 10-a gedəcək; kitab köhnə bucket 1-də saxlanılsa tapılmayacaq.

### Böyümə və kiçilmə nümunəsi

Başlanğıc N=11:

1. 10 kitabda λ=10/11 < 1: dəyişiklik yoxdur.
2. 11-ci unikal insert-dən sonra λ=1: `nextPrime(22)=23`.
3. 11 kitab yenidən hash edilir; n=11 qalır, yeni λ=11/23.
4. Silmələrdən sonra n=6 olduqda 6/23≈0.2609: kiçilmə yoxdur.
5. n=5 olduqda 5/23≈0.2174 < 0.25: yarım 11.5, ən yaxın prime 11.
6. Qalan 5 kitab yeni 11 bucket-ə yerləşdirilir.
7. Sonrakı silmələr N-i başlanğıc minimum olan 11-dən aşağı salmır.

Hər uğurlu əməliyyat üçün ən çox bir resize edilir. Genişlənmə və kiçilmə hədlərinin
fərqli olması sərhəddə hər insert/remove-dan sonra ölçünün irəli-geri dəyişməsini azaldır.
Sabit tutum rejimində hər iki resize yoxlaması söndürülür.

### Statistika metodları

- `size()`: `elementCount_` qaytarır; O(1).
- `capacity()`: vector-un list sayını qaytarır; O(1).
- `loadFactor()`: n və N-i double-a çevirib bölür; integer division yoxdur.
  Məsələn 3/11 nəticəsi 0 deyil, təxminən 0.272727-dir; O(1).
- `collisionCount()`: tarixi sayğacı qaytarır; O(1). Silinmə onu azaltmır.
- `maxChainLength()`: sıfırdan başlayıb bütün bucket-lərin `size()` qiymətlərinin
  maksimumunu `std::max` ilə seçir. Boş cədvəldə 0, vaxt O(N), əlavə yaddaş O(1).

Collision bir uğurlu unikal açarın əvvəldən dolu bucket-ə daxil edilməsidir.
Zəncirdə 1 və ya 100 kitab olması fərq etmir: həmin insert üçün yalnız 1 artırılır.
Bu, müqayisələrin sayı, toqquşan cütlərin sayı və ya cari collision sayı deyil.
Rehash, duplicate, find və remove bu sayğaca daxil edilmir.

## 5. main.cpp — konsol nümayişi

`<iostream>` giriş/çıxış, `<sstream>` tam sətirdən rəqəm oxumaq,
`<string>` mətn və `<exception>` üst səviyyə xəta tutmaq üçündür.
Helper-lər anonymous namespace-də saxlanır.

### readChoice(prompt, minimum, maximum, value)

1. `std::string line` bir giriş sətrini saxlayır.
2. `while (std::cout << prompt, std::getline(std::cin, line))` əvvəl prompt-u yazır,
   sonra sətir oxuyur. Burada vergül operatorunun nəticəsi sağdakı getline-dır.
   EOF zamanı dövr bitir.
3. Hər sətrə yeni `istringstream` qurulur; əvvəlki səhv stream vəziyyəti daşınmır.
4. `input >> candidate` tam ədəd oxumağa çalışır.
5. `!(input >> extra)` rəqəmdən sonra boşluq xaric əlavə simvol olmadığını yoxlayır.
   `1xyz` və `1 2` kimi girişlər rədd olunur; sondakı boşluq qəbul edilir.
6. Minimum və maximum daxil olmaqla sərhəd yoxlanır.
7. Uğurda reference olan value yenilənir və true qaytarılır.
8. Səhv girişdə xəbərdarlıq yazılır və yeni sətir tələb edilir.
9. EOF-də false qaytarılır; sonsuz səhv-giriş dövrü yaranmır.

Çox böyük, int-ə sığmayan ədəd stream extraction-u uğursuz edir və təhlükəsiz rədd olunur.

### readBook(book)

ISBN, title və author ayrı-ayrı `getline` ilə oxunur; daxilində boşluq olan
adlar kəsilmir. Hər girişdə EOF yoxlanır. Available üçün yalnız 0 və 1 qəbul edilir.
Sonda `book.isAvailable = available == 1` yazılır və true qaytarılır.
Yarımçıq girişdə false qaytarılır, main kitabı insert etmir.

### printBook(book)

ISBN, title, author və mövcudluq statusunu yazır.
`book.isAvailable ? "yes" : "no"` bool-u oxunaqlı mətnə çevirir.
Cədvəli dəyişmir; ayrıca lookup da etmir.

### printStatistics(table)

`size`, `capacity`, `loadFactor`, `collisionCount`, `maxChainLength`
metodlarının nəticələrini yazır. Cədvəl const reference ilə ötürülür.

### main() icra sırası

1. `try` bloku proqram səviyyəsində standart istisnaları tutmağa imkan verir.
2. Başlanğıcda 1=Modulo və 2=MAD seçimi alınır; EOF varsa 0 ilə çıxılır.
3. Seçilən mode ilə tutumu 11 olan cədvəl qurulur.
4. `for (;;)` menyunu çıxış seçilənədək təkrarlayır.
5. 1: Book oxunur, insert nəticəsinə görə əlavə edildi və ya duplicate yazılır.
6. 2 və 3: ISBN bir dəfə oxunur. 2 silir; 3 probe sayan axtarış aparır.
7. Axtarış pointer-i null deyilsə `*book` ilə obyekt printBook-a verilir.
   Null olduqda dereference edilmir və “Not found” yazılır.
8. 4: statistika göstərilir.
9. 0 və ya EOF: dövr bitir, destructor-lar işləyir, exit code 0 olur.
10. `std::exception` tutularsa `what()` stderr-ə yazılır və exit code 1 olur.

Menyu mesajları spesifikasiyanın icazə verdiyi kimi ingiliscədir.
Bu, real kitabxana proqramı deyil: interaktiv olaraq yaddaşdakı kataloqu nümayiş etdirir.

## 6. tests/test_chaining.cpp — Release-də də işləyən testlər

Testlər yalnız standart kitabxanadan istifadə edir.
`<cmath>` double müqayisəsində abs, `<limits>` sərhəd qiymətləri,
`<map>` müstəqil ordered-tree reference model, `<random>` sabit seed-li generator,
`<iostream>` PASS/FAIL çıxışı, `<stdexcept>` test xətaları,
`<exception>` exception tutmaq, `<string>` sintetik açarlar üçündür.

`assert` istifadə edilmir: Release-də NDEBUG təyin edildikdə test yoxlamaları itmir.

### Test infrastrukturu funksiyaları

- `require(condition, message)`: şərt false-dursa `runtime_error` atır.
  Şərt true-dursa normal davam edir. Yoxlama hər build konfiqurasiyasında aktivdir.
- `expectThrows<Exception>(action)`: template Exception gözlənən xəta tipidir;
  Function compiler tərəfindən lambda-dan çıxarılır. Action çağırılır, uyğun exception
  tutulsa test uğurla qayıdır. Exception gəlməzsə test özü runtime_error atır.
  Fərqli exception tipi üst test runner-a yayılır və FAIL olur.
- `makeBook(isbn, available=true)`: yalnız sintetik title/author və verilən ISBN-dən
  Book qurur; real şəxsi məlumat istifadə edilmir.
- `checkBook(actual, expected)`: əvvəl pointer-in null olmadığını require ilə yoxlayır;
  yalnız bundan sonra dörd sahəni müqayisə edir. Rehash sahələrdən birini dəyişsə aşkarlayır.
- `fill(table, count)`: 0..count−1 string açarlarını əlavə edir, hər insert-i yoxlayır,
  cüt indekslərdə available=true, tək indekslərdə false saxlayır.
- `referenceMad(hash, a, b, p, capacity)`: əsas koddan fərqli, müstəqil oracle-dır.
  Vurmanı birbaşa etmir. `product=0`, `term=hash%p` ilə başlayır.
  a-nın aşağı biti 1-dirsə term product-a modulo p ilə əlavə edilir.
  Hər dövrdə term ikiqat artırılıb modulo p alınır, a isə 2-yə bölünür.
  Bu, a-nı ikilik təqdimatda hissələrə ayıraraq modular multiplication edir.
  Sonda b və capacity modulu tətbiq olunur. Test p-si 32 bit olduğu üçün
  product+term və term+term də təhlükəsizdir. O(log a) vaxt, O(1) yaddaş.
- `TestCase`: `name` oxunaqlı adı, `void (*run)()` parametrsiz test funksiyasının
  pointer-ini saxlayan struct-dır.
- Test `main()`: TestCase massivini sıra ilə gəzir, `test.run()` çağırır,
  exception yoxdursa PASS yazır. Standart və naməlum exception-ları tutub FAIL
  və mesaj yazır, digər testləri davam etdirir. Sonda `sizeof(cases)/sizeof(cases[0])`
  ilə hal sayı tapılır, uğurlu/test sayı çap edilir. Bir xəta belə varsa exit code 1,
  hamısı uğurludursa 0 qaytarılır. CTest bu exit code-u oxuyur.

### Hər test funksiyasının məqsədi və əsas bloku

| № | Funksiya | Yoxlanan davranış |
|---|---|---|
| 1 | `testEmpty` | Default N=11, n=0, λ=0, collision=0 və maxChain=0 |
| 2 | `testInsertion` | Bir kitabın qəbulu və n-in 1 olması |
| 3 | `testDuplicate` | Eyni ISBN-li fərqli metadata rədd edilir; əvvəlki kitab və statistika qalır |
| 4 | `testLookup` | Başlanğıc sıfırlı/tireli açarla tapılan kitabın bütün sahələri |
| 5 | `testMissingLookup` | Cədvəldə başqa kitab olsa da olmayan ISBN üçün nullptr |
| 6 | `testRemoval` | Uğurlu silinmə, n-in azalması və kitabın daha tapılmaması |
| 7 | `testMissingRemoval` | Olmayan açarın silinməsi false; say, tutum və collision dəyişmir |
| 8 | `testLoadFactor` | 3/11 nəticəsini 1e−12 tolerantlıqla müqayisə edir |
| 9 | `testModulo` | 131%11=10, sıfır hash və N=1 sərhədi |
| 10 | `testMad` | Əl ilə hesablanmış 100,3,7,101,11 nümunəsi; sıfır hash və cached helper uyğunluğu |
| 11 | `testBaseOne` | AB cəmi 131, AB və BA üçün eyni hash |
| 12 | `testBaseThirtySeven` | AB=2471, BA=2507, boş string hash=0 |
| 13 | `testPrimes` | Ayrı dövrlərlə sadə və mürəkkəb nümunələri, 0 və 1-i yoxlayır |
| 14 | `testNextPrime` | Ciddi böyük ilk prime: 0→2, 1→2, 2→3, 11→13, 22→23 |
| 15 | `testForcedCollisions` | Hər iki mode üçün AB, BA, C@ eyni base-1 hash alır və 3 node-luq chain yaradır |
| 16 | `testCollisionCounter` | 3 eyni-bucket insert üçün 2 collision; duplicate/find/remove artırmır, reinsertion artırır |
| 17 | `testMaximumChain` | Müxtəlif bucket-lər və silinmələrlə maximum 2→1→0 olur |
| 18 | `testExpansionThreshold` | 10/11-də resize yoxdur, 11/11-də N=23 olur |
| 19 | `testExpansionPrime` | 3→7→17 ölçüləri, twice-capacity-dən ciddi böyük prime qaydası |
| 20 | `testExpansionPreservesBooks` | Hər iki mode-da 100 insert-dən sonra bütün sahələr yenidən axtarılır |
| 21 | `testShrinkThreshold` | N=23-də n=6 üçün qalır, n=5 üçün 11-ə kiçilir |
| 22 | `testShrinkPreservesBooks` | Hər iki mode-da kiçilmədən sonra qalan 5 kitabın bütün sahələri qorunur |
| 23 | `testMinimumCapacity` | Başlanğıc 10→11 və 1→2 normallaşır, 100 kitab silinəndə minimum 11 qalır |
| 24 | `testFixedCapacity` | Hər mode-da N=3 ilə 30 kitab, λ=10; find/remove işləyir, N dəyişmir; 10007 saxlanır |
| 25 | `testBothModes` | MODULO və MAD üçün insert, duplicate, find, remove, missing-remove müqaviləsi |
| 26 | `testProbes` | Boş chain=0; birinci/ikinci/üçüncü tapıntı=1/2/3; eyni chain miss=3; hər çağırışda reset |
| 27 | `testLargeMad` | UINT64_MAX və böyük a,b,p; 1000 sabit-seed nümunəsini müstəqil referenceMad ilə müqayisə edir |
| 28 | `testThousandsOfKeys` | Hər mode-da 5000 unikal insert, bütün sahələrin lookup-u, 5000 silinmə və minimuma qayıdış |
| 29 | `testInvalidParameters` | N=0, naməlum mode, N>=p, mürəkkəb/kiçik/böyük p, a=0/a>=p, b>=p və MAD tutum sərhədi |
| 30 | `testRepeatedRehash` | Hər mode-da 8 dövr × 120 insert/duplicate/remove; hər turda n=0, N=3 |
| 31 | `testShrinkPrimeRules` | Yaxınlıq, minimum, bərabərlik, səhv minimum; N=3..199 üçün bütün prime-larla brute-force müqayisə |
| 32 | `testUnsignedAndOverflow` | 0xFF→255, base=UINT64_MAX wraparound, base=0, nextPrime maksimum sərhədləri |
| 33 | `testChainRemovalPositions` | Eyni chain-in orta, baş, son və axırıncı node-larının silinməsi |
| 34 | `testRehashCollisionAccounting` | Böyümə və kiçilmə collision tarixçəsini artırmır; qalan kitab tapılır |
| 35 | `testTextKeys` | Boş açar, 001/1 fərqi, daxilində null olan 3 baytlıq string, 10000 baytlıq açar |
| 36 | `testRandomizedReference` | Hər mode üçün 6000 qarışıq əməliyyatın nəticəsi std::map reference-i ilə yoxlanır |

`testLargeMad`-də seed 20261125, `testRandomizedReference`-də 42-dir.
Təsadüfi axın təkrarolunandır; məcburi collision-lar isə təsadüfdən asılı deyil.

Böyük MAD testinin əl ilə sübutu: p=2^32−5 olduğundan 2^32 mod p=5,
2^64 mod p=25 və UINT64_MAX mod p=24.
a=b=p−1 seçilsə `(−24−1) mod p=p−25`; test bunun %97 nəticəsini yoxlayır.

Randomized reference testində switch-in 0 qolu insert-i ordered map-in emplace
nəticəsi ilə, 1 qolu remove-u erase nəticəsi ilə, default qolu find-i map lookup-u
ilə müqayisə edir. Hər addımda ölçülər, sonda bütün qalan kitablar yoxlanır.
`std::map` hazır hash table deyil; yalnız testdə müstəqil düzgünlük modeli kimi işlənir.

## 7. CMakeLists.txt — hədəflər və build qaydaları

1. `cmake_minimum_required(VERSION 3.16)` minimum CMake dil/əmr səviyyəsidir.
2. `project(VSA1_Task1 LANGUAGES CXX)` layihənin C++ istifadə etdiyini bildirir.
3. Mənbə və build qovluğu eynidirsə FATAL_ERROR verilir.
   Həmişə `-B build` və ya onun altqovluğunu istifadə edin.
4. `CMAKE_CXX_STANDARD 17`, REQUIRED ON və EXTENSIONS OFF C++17 tələb edir,
   GNU compiler extensions kimi qeyri-standart dil rejimlərini söndürür.
5. `ENABLE_SANITIZERS` default OFF-dur; əlavə runtime olmadan adi build işləyir.
6. `configure_target(target)` hər hədəfə PRIVATE compiler/linker yoxlamaları verir.
   MSVC: `/W4` xəbərdarlıqları, `/permissive-` daha ciddi uyğunluq,
   `/utf-8` mənbə kodlaması. GCC/Clang: Wall, Wextra, Wpedantic, Wconversion.
7. Sanitizer seçilərsə uyğun platformanın bayraqları əlavə edilir.
   `PRIVATE` bu seçimin həmin hədəfə aid olduğunu bildirir.
8. `add_library(chaining ...)` iki .cpp-dən reusable kitabxana yaradır.
9. `target_include_directories(... PUBLIC .../include)` başlıq yolunu həm kitabxanaya,
   həm onu link edən istehlakçılara ötürür.
10. `add_executable(library_catalogue main.cpp)` demo hədəfini yaradır.
11. `target_link_libraries(... PRIVATE chaining)` demo-nu kitabxanaya bağlayır.
12. Eyni qayda ilə `test_chaining` executable-ı yaradılır.
13. `enable_testing()` CTest-i aktivləşdirir.
14. `add_test(NAME task1_chaining COMMAND test_chaining)` test executable-ını qeydiyyata alır.

Kitabxana hədəfi demo-nun main funksiyasını daşımır.
Buna görə digər komanda üzvü HashUtils və SeparateChainingHash-dan öz test və
benchmark executable-ında ayrıca istifadə edə bilər.

## 8. .gitignore və README.md

`.gitignore` mənbəni dəyişmir və Git əməliyyatı icra etmir.
Sadəcə sonrakı komanda inteqrasiyasında build artefaktlarının izlənməməsini göstərir:

- `build/` və nested build qovluqları bütün yerli artefaktları, endirilmiş alətləri,
  logları və müvəqqəti test fayllarını istisna edir.
- exe/obj/o/lib/a/dll/so/dylib binary və object/library fayllarıdır.
- pdb/ilk/idb/exp/pch/tlog compiler, linker və debug köməkçi fayllarıdır.
- CMakeCache, CMakeFiles, Testing, Makefile və generated project faylları
  təkrar yaradıla bilən build konfiqurasiyasıdır.
- `.vs/`, `.vscode/`, tmp/log və `*~` yerli IDE və müvəqqəti fayllardır.

README bu arxitekturanı, icra komandalarını, hər funksiyanın məqsədini, mühəndislik
qərarlarını, nümunələri və real yoxlama nəticələrini saxlayır.

## Vaxt və yaddaş mürəkkəbliyi

L: ISBN-in uzunluğu; C: seçilmiş chain-in uzunluğu; N: bucket sayı;
n: kitab sayı; S: bütün kitabların string məlumatlarının ümumi ölçüsü;
B: bir kitabın ISBN+title+author mətn həcmi.

| Əməliyyat | Vaxt | Əlavə yaddaş |
|---|---|---|
| polynomialHash | O(L) | O(1) |
| moduloCompress | O(1) | O(1) |
| cached MadCompressor::compress | O(1) | O(1) |
| MadCompressor konstruktoru / sərbəst madCompress | O(sqrt(p)) validasiya | O(1) |
| find / resize-siz remove | O(L + C×L), açar müqayisəsinin worst-case qiyməti ilə | O(1) |
| resize-siz uğurlu insert | O(L + C×L + B) | Yeni node və onun mətnləri üçün O(B) |
| size / capacity / loadFactor / collisionCount | O(1) | O(1) |
| maxChainLength | O(N) | O(1) |
| rehash | O(N_old + N_new + n + S) | Köhnə cədvələ əlavə O(N_new + n + S) |
| isPrime(M) | O(sqrt(M)) | O(1) |
| nextPrime / shrinkPrimeCapacity | Yoxlanan prime namizədlərinin sayından asılıdır | O(1) |

Yaxşı paylanmada C-nin gözlənilən qiyməti təxminən λ-dır.
Sabit uzunluqlu ISBN və idarə olunan load factor ilə find/remove/insert-in
resize-siz gözlənilən qiyməti O(1)-dir.
Hamısı bir bucket-ə düşərsə C=n və axtarış O(n), string xərci də sayılarsa O(nL)-dir.
Böyümə zamanı tək insert və kiçilmə zamanı tək remove bütün cədvəlin rehash xərcini daşıyır.
Prime seçimi üçün əlavə axtarış xərci də var; textbook amortized O(1) şərhi
yaxşı hashing, sabit ölçülü açarlar və geometrik resizing fərziyyələrinə əsaslanır.

Sabit tutum rejimində λ böyüdükcə expected chain uzunluğu da artır; O(1) gözləntisi
limitsiz n üçün tətbiq edilə bilməz. Cədvəlin daimi yaddaşı O(N+n+S)-dir.

### RAII və yaddaşın sahibi

Vector, list və string resurslarını öz destructor-ları azad edir.
Müvəqqəti rehash vector-u exception zamanı da təmizlənir.
`findByISBN` yalnız borrowed pointer verir; ayrıca allocation etmir.
Cədvəlin normal scope-dan çıxması bütün kitabları azad edir.
Bu implementasiyada raw `new`, `delete` və manual allocator yoxdur.

## Mühəndislik qərarları və sərhədlər

1. Başlanğıc tutum yuxarıdakı ilk prime-a normallaşdırılır; sıfır rədd edilir.
2. Minimum ölçü həmin normallaşdırılmış başlanğıcdır.
3. Shrink exact N/2-yə ən yaxın prime-dır; bərabərlikdə kiçik prime seçilir.
4. Hər uğurlu insert/remove ən çox bir resize başladır.
5. MAD üçün p 32 bitlə məhdudlaşır; table-də böyük sabit prime istifadə olunur.
   N >= p olacaq böyümə rədd edilir və insert rollback olunur.
   Dəyişməz p/a/b sayəsində resize parametr dəyişməsindən əlavə qeyri-müəyyənlik yaratmır.
6. Rehash kitabları kopyalayır: exception zamanı köhnə vəziyyət qorunur, əvəzində
   müvəqqəti yaddaş sərfi artır və əvvəlki pointer-lər etibarsızlaşır.
7. Insert collision sayğacı cumulative-dır və rehash zamanı sıfırlanmır.
8. Chain sırası push_back ilə saxlanır; probe konkret ISBN müqayisəsidir.
9. Probe sayı `int` sərhədini aşarsa overflow_error atılır; collision sayğacı da yoxlanır.
10. ISBN formatı, trim, checksum və Unicode normalization aparılmır.
11. Baza 0 və 1 daxil olmaqla bütün uint64 bazalar qəbul edilir; zəif baza seçimi
    correctness-i pozmur, performansı pisləşdirə bilər.
12. Sinfin adi value copy-si standart konteynerlərin dərin kopyasını alır.
    Move-dan sonra mənbə obyektə yalnız məhv etmə və yenidən assignment tətbiq edin;
    onun boşalmış vector-u üzərində axtarış/statistika müqaviləsi verilmir.
13. Multi-thread istifadəsi üçün sinxronizasiya yoxdur.
14. Böyük tutumlar üçün yaddaş limiti və sadəlik yoxlamasının qiyməti praktik sərhəddir.
15. Task 2, tam Task 3 benchmark-u, diskə persistence və GUI həyata keçirilməyib.

## Komandanın gələcək inteqrasiyası

Bu sessiyada repository yaratmaq, commit, remote, push və GitHub bağlantısı yoxdur.
İnteqrasiya ayrıca nəzərdən keçirildikdən sonra komanda tərəfindən edilə bilər:

1. `include/Book.h`, `include/HashUtils.h`, `include/SeparateChainingHash.h`,
   `src/HashUtils.cpp`, `src/SeparateChainingHash.cpp` və test faylını mövcud
   ortaq layihənin uyğun qovluqlarına daşıyın; əvvəl ad konfliktlərini yoxlayın.
2. Ortaq CMake-də `chaining` kitabxanasını bir dəfə qeydiyyata alın.
3. Task 2 eyni HashUtils modulunu istifadə etsin; ikinci kopya yaratmayın.
4. Hər executable-a yalnız öz main funksiyasını link edin.
5. Task 3-də `SeparateChainingHash(10007, mode, base, false)` ilə sabit tutum,
   `base=1` və `base=37` seçimi, collisionCount və probe overload-u istifadə edilə bilər.
6. Ölçmə müqayisəsində probe və collision təriflərini əvvəl uyğunlaşdırın.
7. Build qovluğunu, portativ alətləri, executable-ları və logları mənbəyə daxil etməyin.
8. Dəyişiklikdən sonra Debug və Release testlərini ortaq layihədə yenidən işlədin.

## Nəzəri mənbələr haqqında qeyd

Tapşırığın verdiyi nəzəri istiqamətlər: Mark Allen Weiss, *Data Structures and
Algorithm Analysis in C++*, 4-cü nəşr, fəsil 5 və §5.3; Goodrich və b.,
*Data Structures and Algorithms in Python*, fəsil 10, §10.2, C-10.30 və P-10.51.
Kitabların tam mətnləri bu sessiyada təqdim edilməyib və həmin hissələrin
birbaşa oxunduğu iddia edilmir. Reallaşdırma təqdim edilmiş spesifikasiyaya əsaslanır.


## Faktiki yoxlama nəticələri — 25 noyabr 2026

Yoxlamalar bu qovluqda və sintetik məlumatlarla icra edilib:

| Yoxlama | Faktiki nəticə |
|---|---|
| Birbaşa MSVC Debug, /W4 /RTC1 | 36/36 PASS, exit code 0 |
| Birbaşa MSVC Release, /O2 /DNDEBUG | 36/36 PASS, exit code 0 |
| CMake configure | Visual Studio 18 2026, x64 — uğurlu |
| CMake Debug build | chaining.lib, library_catalogue.exe, test_chaining.exe — uğurlu |
| CTest Debug | 1/1 PASS; test müddəti 0.43 saniyə |
| Debug test executable, ayrıca icra | 36/36 PASS |
| CMake Release build | Hər üç hədəf uğurlu |
| CTest Release | 1/1 PASS; test müddəti 0.24 saniyə |
| Release test executable, ayrıca icra | 36/36 PASS |
| CMake Debug/Release konsol ssenariləri | Hər iki mode üçün insert, duplicate, find, remove, statistika, səhv giriş və EOF — PASS |
| MSVC /analyze /analyze:quiet | İki library mənbəyi, main və test mənbəyi; diaqnostikasız, exit code 0 |
| Debug/Release build loglarında compiler warning/error yoxlaması | Heç biri tapılmadı |
| AddressSanitizer | İcra edilməyib: quraşdırılmış MSVC alətlərində ASan runtime komponentləri tapılmadı |
| GCC/Clang və digər əməliyyat sistemləri | Bu sessiyada test edilməyib |

Compiler identifikasiyası: **MSVC 19.51.36260.0**;
toolset qovluğu **14.51.36231**; Windows SDK **10.0.26100.0**.
CMake **4.5.0** portativdir, sistem PATH-i və quraşdırılmış proqramlar dəyişdirilməyib.

CMake [rəsmi paylama serverindən](https://cmake.org/download/) endirilib.
PowerShell endirməsi irəliləmədiyi, biraxınlı curl endirməsi isə yavaşlıqdan
180 saniyə limitinə çatdığı üçün hissəli HTTP range endirməsi istifadə edilib.
Birləşdirilmiş arxivin SHA-256 qiyməti rəsmi siyahı ilə yoxlanıb:

```text
301b1728317ccd84e984b02e366a8293735bc5ac618757d3d8f58b578f062411
```

Endirmə problemi mənbə kodundakı xəta deyildi. Faktiki compilation və correctness
yoxlamalarında düzəliş tələb edən xəta aşkar edilməyib.

### Bu kompüter üçün hazır komandalar

VS Code terminalını VSA-LAB4TEST kökündə açın. CMake PATH-də deyil; portativ yolu istifadə edin:

```powershell
$cmakeExe = ".\build\tools\cmake-4.5.0-windows-x86_64\bin\cmake.exe"
$ctestExe = ".\build\tools\cmake-4.5.0-windows-x86_64\bin\ctest.exe"

# Compiler-in müvəqqəti artefaktlarını da build daxilində saxlayır.
New-Item -ItemType Directory -Path ".\build\tmp" -Force | Out-Null
$env:TEMP = Join-Path (Get-Location) "build\tmp"
$env:TMP = $env:TEMP

& $cmakeExe -S . -B build -G "Visual Studio 18 2026" -A x64
& $cmakeExe --build build --config Debug --parallel 2
& $ctestExe --test-dir build -C Debug --output-on-failure
.\build\Debug\test_chaining.exe
.\build\Debug\library_catalogue.exe

& $cmakeExe --build build --config Release --parallel 2
& $ctestExe --test-dir build -C Release --output-on-failure
.\build\Release\library_catalogue.exe
```

Bu TEMP/TMP dəyişiklikləri yalnız həmin terminal prosesinə və onun child proseslərinə aiddir.
Yeni terminal istifadəçinin əvvəlki sistem konfiqurasiyası ilə açılır.

### Saxlanmış yoxlama sübutları

- `build/configure.log`: compiler, SDK və CMake konfiqurasiyası.
- `build/build-Debug.log`, `build/build-Release.log`: faktiki build çıxışı.
- `build/ctest-Debug.log`, `build/ctest-Release.log`: CTest nəticələri.
- `build/tests-Debug.log`, `build/tests-Release.log`: bütün 36 PASS sətri.
- `build/menu-Debug-mode-1.log`, `build/menu-Debug-mode-2.log`,
  `build/menu-Release-mode-1.log`, `build/menu-Release-mode-2.log`: demo ssenariləri.
- `build/analysis/`: MSVC statik analizi ilə yaranmış object faylları.
- `build/tools/`: portativ CMake, arxiv və rəsmi checksum siyahısı.

### Yaradılmış və dəyişdirilmiş fayllar

Yeni: `include/Book.h`, `include/HashUtils.h`, `include/SeparateChainingHash.h`,
`src/HashUtils.cpp`, `src/SeparateChainingHash.cpp`, `tests/test_chaining.cpp`,
`main.cpp`, `CMakeLists.txt`, `.gitignore`.

Yenilənmiş: mövcud `README.md`; əvvəlki layihə başlığı saxlanıb və bu sənədlə genişləndirilib.

Bütün törəmə artefaktlar `build/` altındadır. AGENTS.md, nested layihə və yeni Git
repository yaradılmayıb. Task 2 və tam Task 3 başlanmayıb.

