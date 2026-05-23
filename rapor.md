# BİLGİSAYAR MÜHENDİSLİĞİ
# SİSTEM PROGRAMLAMA 2025-2026 BAHAR DÖNEMİ PROJESİ
## tarsau — Sıkıştırmasız Arşivleme Programı

---

## İçindekiler

1. Proje Açıklaması
2. Geliştirme Ortamı
3. Proje Mimarisi ve Dosya Yapısı
4. .sau Arşiv Dosya Formatı
5. Fonksiyonların Açıklaması
6. Kod Parçacıkları
7. Makefile
8. Test ve Ekran Çıktıları
9. Sonuç

---

## 1. Proje Açıklaması

Bu projede, tar, rar veya zip gibi çalışan ancak sıkıştırma yapmayan **tarsau** adlı bir arşivleme programı C dilinde geliştirilmiştir. Program, Linux (POSIX) işletim sistemi üzerinde çalışmakta olup komut satırından aşağıdaki şekillerde kullanılmaktadır:

```
tarsau -b dosya1 dosya2 ... [-o cikti.sau]   # Arşivleme (bundle)
tarsau -a arsiv.sau [dizin]                   # Açma (extract)
```

Program yalnızca **ASCII metin dosyalarını** kabul etmekte; bu dosyaları tek bir `.sau` uzantılı arşiv dosyasında birleştirmekte ve gerektiğinde tekrar açabilmektedir.

---

## 2. Geliştirme Ortamı

| Özellik | Değer |
|---------|-------|
| İşletim Sistemi | Ubuntu 22.04 LTS (WSL2 üzerinde) |
| Programlama Dili | C (ISO C11) |
| Derleyici | GCC 11.x |
| Derleme Araçları | GNU Make |
| Standart | POSIX.1-2017 |
| Derleme Bayrakları | `-Wall -Wextra -std=c11 -pedantic` |

---

## 3. Proje Mimarisi ve Dosya Yapısı

```
g221210057/
├── tarsau.c          ← Tek kaynak dosyası (tüm program)
├── Makefile          ← Derleme ve test kuralları
├── lib/
│   └── tarsau.o     ← Nesne dosyası (make derle sonrası)
└── bin/
    └── tarsau       ← Çalıştırılabilir program (make bagla sonrası)
```

Program tek kaynak dosyasından oluşmakta olup içerisindeki modüller şunlardır:

| Modül / Fonksiyon | Görev |
|-------------------|-------|
| `is_text_file()` | ASCII metin dosyası kontrolü |
| `get_file_size()` | Dosya boyutunu okuma |
| `get_file_mode()` | Dosya izinlerini okuma |
| `mkdirs()` | Özyinelemeli dizin oluşturma |
| `file_basename()` | Dosya yolundan sadece adı alma |
| `do_bundle()` | `-b` arşivleme işlemi |
| `do_extract()` | `-a` açma işlemi |
| `main()` | Parametre yönlendirme |

---

## 4. .sau Arşiv Dosya Formatı

Arşiv dosyası iki ana bölümden oluşur:

```
BÖLÜM 1: ORGANİZASYON
  [10 bayt]      → Organizasyon bölümünün boyutu (ASCII, sıfır dolgulu)
                   Örnek: 0000000055
  [org_len bayt] → Kayıt listesi:
                   |dosya1,izin,boyut|dosya2,izin,boyut|...|

BÖLÜM 2: ARŞİVLENMİŞ DOSYALAR
  [dosya1 içeriği][dosya2 içeriği]...
  (ayraç kullanılmaz; boyut bilgisi org bölümünden okunur)
```

### Örnek .sau Dosyası (ham içerik):

```
0000000055|test1.txt,0777,48|test2.txt,0777,41|test3.txt,0777,28|Merhaba Dunya!
Bu bir test dosyasidir.
Satir 3.
Ikinci test dosyasinin icerigi.
Satir 2.
Ucuncu ve son test dosyasi.
```

### Kayıt Formatı:

```
|<dosya_adi>,<izinler_octal>,<boyut_bayt>|
```

Örnek: `|test1.txt,0777,48|`

---

## 5. Fonksiyonların Açıklaması

### 5.1. `is_text_file(const char *path)`

Bir dosyanın geçerli ASCII metin dosyası olup olmadığını byte byte kontrol eder.

Kabul edilen karakterler:
- `0x09` TAB (Yatay sekme)
- `0x0A` LF (Satır sonu)
- `0x0D` CR (Taşıma dönüşü)
- `0x20 – 0x7E` Yazdırılabilir ASCII

Reddedilen karakterler:
- `0x00` NULL byte
- `0x01 – 0x08`, `0x0B`, `0x0C`, `0x0E – 0x1F` (kontrol karakterleri)
- `0x7F` DEL
- `0x80 – 0xFF` (ASCII dışı)

Dönüş değerleri: `1` = metin, `0` = binary/uyumsuz, `-1` = açılamadı

---

### 5.2. `mkdirs(const char *path)`

`mkdir -p` davranışını taklit eden, özyinelemeli dizin oluşturucu fonksiyondur. Verilen yoldaki tüm ara dizinleri sırayla oluşturur; varsa `EEXIST` hatasını görmezden gelir.

---

### 5.3. `do_bundle(int argc, char *argv[])`

`-b` parametresiyle çağrılan arşivleme fonksiyonudur.

İşlem adımları:
1. Komut satırı argümanları ayrıştırılır (`-o` parametresi varsa çıktı adı belirlenir)
2. Her dosya için: boyut kontrolü, ASCII kontrolü yapılır
3. Toplam boyut 200 MB sınırı kontrol edilir
4. Organizasyon bölümü bellekte oluşturulur
5. Arşiv dosyasına: 10 baytlık başlık, org bölümü ve dosya içerikleri sırayla yazılır

---

### 5.4. `do_extract(int argc, char *argv[])`

`-a` parametresiyle çağrılan açma fonksiyonudur.

İşlem adımları:
1. `.sau` uzantısı ve dosya varlığı kontrol edilir
2. 10 baytlık başlık okunur; yalnızca rakam içerip içermediği doğrulanır
3. Organizasyon bölümü okunur ve `|` ayraçlarıyla ayrıştırılır
4. Hedef dizin varsa oluşturulur (`mkdirs`)
5. Her dosya için: boyut kadar byte arşivden okunur, hedefe yazılır
6. Dosyanın orijinal izinleri `chmod()` ile geri yüklenir

---

## 6. Kod Parçacıkları

### 6.1. ASCII Metin Dosyası Kontrolü

```c
static int is_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        unsigned char uc = (unsigned char)ch;
        if (uc == 0x09 || uc == 0x0A || uc == 0x0D)
            continue;
        if (uc >= 0x20 && uc <= 0x7E)
            continue;
        fclose(fp);
        return 0;   /* binary / uyumsuz */
    }
    fclose(fp);
    return 1;       /* metin dosyasi */
}
```

### 6.2. Organizasyon Bölümü Oluşturma

```c
size_t max_org = (size_t)file_count * MAX_ORG_ENTRY + 16;
char  *org     = (char *)malloc(max_org);
org[0] = '\0';

for (int i = 0; i < file_count; i++) {
    const char *bname = file_basename(files[i]);
    long        sz    = get_file_size(files[i]);
    mode_t      mode  = get_file_mode(files[i]);
    char        entry[PATH_MAX + 40];

    snprintf(entry, sizeof(entry), "|%s,%04o,%ld", bname, (unsigned)mode, sz);
    strncat(org, entry, max_org - strlen(org) - 1);
}
strncat(org, "|", max_org - strlen(org) - 1);

/* 10 baytlik baslik + org bolumu */
fprintf(out, "%010lu", (unsigned long)strlen(org));
fwrite(org, 1, strlen(org), out);
```

### 6.3. Arşivden Dosya Çıkarma ve İzin Geri Yükleme

```c
long remaining = entries[i].size;
char buf[8192];
while (remaining > 0) {
    size_t to_read = (sizeof(buf) < (size_t)remaining)
                     ? sizeof(buf) : (size_t)remaining;
    size_t n = fread(buf, 1, to_read, fp);
    if (n == 0) break;
    fwrite(buf, 1, n, outfp);
    remaining -= (long)n;
}
fclose(outfp);

/* Orijinal izinleri geri yukle */
chmod(filepath, entries[i].mode);
```

### 6.4. Özyinelemeli Dizin Oluşturma

```c
static int mkdirs(const char *path)
{
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') tmp[--len] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}
```

---

## 7. Makefile

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -pedantic
TARGET  = tarsau
SRC     = tarsau.c

.PHONY: all derle bagla calistir temizle test

all: derle bagla calistir

derle:
    @mkdir -p lib
    @echo "Derleniyor..."
    $(CC) $(CFLAGS) -c $(SRC) -o ./lib/tarsau.o

bagla:
    @mkdir -p bin
    @echo "Baglaniyor..."
    $(CC) ./lib/tarsau.o -o ./bin/$(TARGET)

calistir:
    @echo "Calistiriliyor..."
    -./bin/$(TARGET)

temizle:
    @rm -rf lib bin *.sau
    @echo "Temizlendi."
```

Kullanım:

```bash
make          # Derle + Bağla + Çalıştır
make derle    # Sadece nesne dosyası üret → lib/tarsau.o
make bagla    # Sadece bağla → bin/tarsau
make calistir # Sadece çalıştır
make test     # Tüm test senaryolarını çalıştır
make temizle  # Üretilen dosyaları sil
```

---

## 8. Test ve Ekran Çıktıları

### 8.1. Derleme

```
Derleniyor...
gcc -Wall -Wextra -std=c11 -pedantic -c tarsau.c -o ./lib/tarsau.o
Baglaniyor...
gcc ./lib/tarsau.o -o ./bin/tarsau
```

### 8.2. Arşivleme (Test 2)

```
./bin/tarsau -b test1.txt test2.txt test3.txt -o test.sau
Arsiv dosyasi 'test.sau' basariyla olusturuldu.

=== Arsiv dosyasi ham icerigi ===
0000000055|test1.txt,0777,48|test2.txt,0777,41|test3.txt,0777,28|Merhaba Dunya!
Bu bir test dosyasidir.
Satir 3.
Ikinci test dosyasinin icerigi.
Satir 2.
Ucuncu ve son test dosyasi.
```

### 8.3. Arşiv Açma (Test 3)

```
./bin/tarsau -a test.sau test_output
Arsiv 'test.sau' basariyla acildi -> test_output
Cikarilan dosyalar (3):
  test1.txt  (48 bayt, izin: 0777)
  test2.txt  (41 bayt, izin: 0777)
  test3.txt  (28 bayt, izin: 0777)
```

### 8.4. İçerik Doğrulama — diff (Test 4)

```
  test1.txt : BASARILI
  test2.txt : BASARILI
  test3.txt : BASARILI
```

### 8.5. Binary Dosya Reddi (Test 6)

```
binary.bin giris dosyasinin formati uyumsuzdur!
  Binary dosya reddedildi: BASARILI
```

### 8.6. Geçersiz Arşiv Hatası (Test 7)

```
Arsiv dosyasi uygunsuz veya bozuk!
  Gecersiz arsiv reddedildi: BASARILI
```

### 8.7. Tüm Test Sonuçları

```
============================================
 Tum testler tamamlandi!
============================================

Test 1 — Metin dosyaları oluşturma    : BAŞARILI
Test 2 — Arşivleme (-b)               : BAŞARILI
Test 3 — Arşiv açma (-a)              : BAŞARILI
Test 4 — diff ile içerik doğrulama    : BAŞARILI (3/3)
Test 5 — Varsayılan çıktı a.sau       : BAŞARILI
Test 6 — Binary dosya reddi           : BAŞARILI
Test 7 — Geçersiz arşiv hatası        : BAŞARILI
```

---

## 9. Sonuç

Bu proje kapsamında, POSIX uyumlu C dilinde çalışan, sıkıştırma yapmayan bir arşivleme programı başarıyla geliştirilmiştir.

| Gereksinim | Durum |
|-----------|-------|
| Yalnızca ASCII metin dosyalarını kabul etme | Karşılandı |
| En fazla 32 dosya arşivleme | Karşılandı |
| Toplam boyut 200 MB sınırı | Karşılandı |
| Varsayılan çıktı dosyası `a.sau` | Karşılandı |
| Arşiv formatı: 10 bayt başlık + org bölümü + dosyalar | Karşılandı |
| `-a` ile çıkarma ve hedef dizin oluşturma | Karşılandı |
| Orijinal dosya izinlerinin korunması | Karşılandı |
| Hatalı giriş dosyası için Türkçe hata mesajı | Karşılandı |
| Geçersiz arşiv için hata mesajı | Karşılandı |
| `make` ile derlenebilirlik | Karşılandı |

---

*Bilgisayar Mühendisliği — Sistem Programlama 2025-2026 Bahar Dönemi*  
*Teslim Tarihi: 24 Mayıs 2026*
