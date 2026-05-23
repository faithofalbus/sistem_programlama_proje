# BİLGİSAYAR MÜHENDİSLİĞİ — SİSTEM PROGRAMLAMA
## 2025-2026 Bahar Dönemi Projesi
### tarsau — Sıkıştırmasız Arşivleme Programı

**GitHub:** https://github.com/faithofalbus/sistem_programlama_proje  
**Teslim Tarihi:** 24 Mayıs 2026

---

## 1. Proje Açıklaması

Bu projede, `tar` benzeri ancak sıkıştırma yapmayan **tarsau** adlı bir arşivleme programı C dilinde geliştirilmiştir. Program Linux/POSIX ortamında çalışır ve yalnızca ASCII metin dosyalarını destekler.

```
tarsau -b dosya1 dosya2 ... [-o cikti.sau]   # Arşivleme
tarsau -a arsiv.sau [dizin]                   # Açma
```

**Geliştirme Ortamı:** GCC 11, GNU Make, Ubuntu 22.04 (WSL2), ISO C11, `-Wall -Wextra -std=c11 -pedantic`

---

## 2. .sau Arşiv Formatı

```
[10 bayt] → Organizasyon bölümünün boyutu (ASCII, sıfır dolgulu)
[N bayt]  → |dosya1,izin,boyut|dosya2,izin,boyut|...|
[...]     → Dosya içerikleri art arda, ayraçsız
```

**Örnek:**
```
0000000046|t1.txt,0755,11|t2.txt,0644,11|t3.txt,0644,10|Satir bir.
Satir iki.
Satir uc.
```

---

## 3. Temel Kod Parçacıkları

**ASCII Metin Kontrolü** — Kabul: TAB(9), LF(10), CR(13), 0x20–0x7E. Diğer tüm byte'lar (NULL, kontrol karakterleri, non-ASCII) reddedilir:

```c
static int is_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        unsigned char uc = (unsigned char)ch;
        if (uc == 0x09 || uc == 0x0A || uc == 0x0D) continue;
        if (uc >= 0x20 && uc <= 0x7E) continue;
        fclose(fp); return 0;  /* binary */
    }
    fclose(fp); return 1;  /* metin */
}
```

**Organizasyon Bölümü Yazma:**

```c
size_t max_org = (size_t)file_count * MAX_ORG_ENTRY + 16;
char *org = malloc(max_org);
for (int i = 0; i < file_count; i++) {
    snprintf(entry, sizeof(entry), "|%s,%04o,%ld",
             file_basename(files[i]), get_file_mode(files[i]), get_file_size(files[i]));
    strncat(org, entry, max_org - strlen(org) - 1);
}
strncat(org, "|", max_org - strlen(org) - 1);
fprintf(out, "%010lu", (unsigned long)strlen(org));  /* 10 bayt başlık */
fwrite(org, 1, strlen(org), out);
```

**İzin Geri Yükleme:**

```c
chmod(filepath, entries[i].mode);  /* orijinal izinler geri yüklenir */
```

---

## 4. Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic

all: derle bagla calistir

derle:
    @mkdir -p lib && $(CC) $(CFLAGS) -c tarsau.c -o ./lib/tarsau.o

bagla:
    @mkdir -p bin && $(CC) ./lib/tarsau.o -o ./bin/tarsau

calistir:
    -./bin/tarsau

temizle:
    @rm -rf lib bin *.sau
```

---

## 5. Test Çıktıları

**Arşivleme:**
```
$ ./bin/tarsau -b t1.txt t2.txt t3.txt -o test.sau
Arşiv dosyası 'test.sau' başarıyla oluşturuldu.
```

**Açma:**
```
$ ./bin/tarsau -a test.sau cikar/
Arşiv 'test.sau' başarıyla açıldı -> cikar/
Cikarilan dosyalar (3):
  t1.txt  (11 bayt, izin: 0755)
  t2.txt  (11 bayt, izin: 0644)
  t3.txt  (10 bayt, izin: 0644)
```

**Uyumsuz dosya:**
```
$ ./bin/tarsau -b binary.bin
binary.bin giriş dosyasının formatı uyumsuzdur!
```

**Geçersiz arşiv:**
```
$ ./bin/tarsau -a gecersiz.sau
Arşiv dosyası uygunsuz veya bozuk!
```

---

## 6. Gereksinim Karşılama Tablosu

| Gereksinim | Durum |
|-----------|:-----:|
| Yalnızca ASCII metin dosyaları | ✓ |
| En fazla 32 dosya | ✓ |
| Toplam boyut ≤ 200 MB | ✓ |
| Varsayılan çıktı `a.sau` | ✓ |
| 10 bayt başlık + `\|ad,izin,boyut\|` formatı | ✓ |
| Dosyaları belirtilen dizine açma | ✓ |
| Olmayan dizini otomatik oluşturma | ✓ |
| Göreceli ve mutlak yol desteği | ✓ |
| Orijinal dosya izinlerinin korunması | ✓ |
| Türkçe hata mesajları | ✓ |
| `make` ile derleme | ✓ |
| Program çökmez, temiz çıkış | ✓ |

---

*Bilgisayar Mühendisliği — Sistem Programlama 2025-2026 Bahar Dönemi*  
*GitHub: https://github.com/faithofalbus/sistem_programlama_proje*
