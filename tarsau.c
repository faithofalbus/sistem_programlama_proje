/*
 * tarsau - Sikistirmasiz Arsivleme Programi
 *
 * Kullanim:
 *   tarsau -b dosya1 dosya2 ... [-o cikti.sau]
 *   tarsau -a arsiv.sau [dizin]
 *
 * Bilgisayar Muhendisligi - Sistem Programlama 2025-2026 Bahar Donemi Projesi
 *
 * .sau Arsiv Formati:
 *   [10 bayt: organizasyon boyutu (ASCII, sifir dolgulu)]
 *   [|dosyaadi,izinler(octal 4 rakam),boyut| ... |]
 *   [dosya1 icerigi][dosya2 icerigi]...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>

#define MAX_FILES        32
#define MAX_TOTAL_SIZE   (200L * 1024L * 1024L)   /* 200 MB */
#define ORG_HEADER_LEN   10                         /* ilk 10 bayt: org boyutu */
#define MAX_ORG_ENTRY    (NAME_MAX + 40)            /* her kayit icin max boyut */

/* ================================================================== */
/*  Yardimci fonksiyonlar                                              */
/* ================================================================== */

/*
 * Bir dosyanin gecerli bir ASCII metin dosyasi olup olmadigini kontrol eder.
 *
 * Kabul edilen karakterler:
 *   0x09  HT  (yatay sekme)
 *   0x0A  LF  (satir besleme)
 *   0x0D  CR  (tasima donusu)
 *   0x20-0x7E yazdirilebilir ASCII
 *
 * Diger tum byte degerleri (NULL, kontrol karakterleri, 0x7F DEL,
 * 0x80-0xFF non-ASCII) metin dosyasi olmadigi anlamina gelir.
 *
 * Donus:  1 = metin dosyasi
 *         0 = metin dosyasi degil (binary veya uyumsuz format)
 *        -1 = dosya acilamadi
 */
static int is_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        unsigned char uc = (unsigned char)ch;
        /* Kabul: TAB(9), LF(10), CR(13), printable(32-126) */
        if (uc == 0x09 || uc == 0x0A || uc == 0x0D)
            continue;
        if (uc >= 0x20 && uc <= 0x7E)
            continue;
        /* Diger her sey (NULL, kontrol kar., DEL, non-ASCII) reddedilir */
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

/* Dosya boyutunu dondurur; hata durumunda -1 */
static long get_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -1L;
    return (long)st.st_size;
}

/* Dosya izin bitlerini dondurur (alt 12 bit, rwxrwxrwx + suid/sgid/sticky) */
static mode_t get_file_mode(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0644;
    return st.st_mode & 07777;
}

/*
 * Bir dizin agacini ozyinelemeli olarak olusturur (mkdir -p benzeri).
 * Donus:  0 = basarili,  -1 = hata
 */
static int mkdirs(const char *path)
{
    char tmp[PATH_MAX];
    char *p;
    size_t len;

    if (!path || *path == '\0')
        return -1;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    /* Sondaki egik cizgiyi kaldir */
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }

    /* Her ara dizini olustur */
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }

    /* Son dizini olustur */
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;

    return 0;
}

/*
 * Verilen dosya yolundan sadece dosya adini (basename) dondurur.
 * Statik tampon kullaniyor — tek seferlik kullanim icin uygundur.
 */
static const char *file_basename(const char *path)
{
    /* libgen.h basename() path'i degistirebilir; kopya uzerinde calis */
    static char buf[PATH_MAX];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return basename(buf);   /* POSIX basename — her zaman gecerli pointer dondurur */
}

/* ================================================================== */
/*  tarsau -b : Dosyalari arsivle (bundle)                            */
/* ================================================================== */
static int do_bundle(int argc, char *argv[])
{
    const char *files[MAX_FILES];
    int         file_count = 0;
    const char *output     = "a.sau";   /* varsayilan cikti */

    /* --- Argumanlari ayristir --- */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output = argv[++i];
            } else {
                fprintf(stderr,
                        "Hata: -o parametresinden sonra dosya adi belirtilmedi!\n");
                return 1;
            }
        } else {
            if (file_count >= MAX_FILES) {
                fprintf(stderr,
                        "Hata: En fazla %d dosya arsivlenebilir!\n", MAX_FILES);
                return 1;
            }
            files[file_count++] = argv[i];
        }
    }

    if (file_count == 0) {
        fprintf(stderr, "Hata: Arsivlenecek dosya belirtilmedi!\n");
        return 1;
    }

    /* --- Dosyalari dogrula --- */
    long total_size = 0;

    for (int i = 0; i < file_count; i++) {
        long sz = get_file_size(files[i]);
        if (sz < 0) {
            /* Dosya acilamiyor ya da bulunamiyor */
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", files[i]);
            return 1;
        }

        int text = is_text_file(files[i]);
        if (text == -1) {
            /* Dosya acilamiyor */
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", files[i]);
            return 1;
        }
        if (text == 0) {
            /* Binary veya uyumsuz format */
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", files[i]);
            return 1;
        }

        total_size += sz;
    }

    if (total_size > MAX_TOTAL_SIZE) {
        fprintf(stderr,
                "Hata: Giris dosyalarinin toplam boyutu 200 MB'i asiyor!\n");
        return 1;
    }

    /*
     * --- Organizasyon (icerik) bolumunu olustur ---
     *
     * Format: |dosyaadi,izin,boyut|dosyaadi,izin,boyut|
     *
     * Sadece dosya adi (basename) saklanir; tam yol degil.
     * Bu sayede -a ile acildiginda hedef dizinde dogru konuma yazilir.
     */
    size_t max_org = (size_t)file_count * MAX_ORG_ENTRY + 16;
    char  *org     = (char *)malloc(max_org);
    if (!org) {
        perror("malloc");
        return 1;
    }
    org[0] = '\0';

    for (int i = 0; i < file_count; i++) {
        const char *bname = file_basename(files[i]);
        long        sz    = get_file_size(files[i]);
        mode_t      mode  = get_file_mode(files[i]);
        char        entry[PATH_MAX + 40];

        snprintf(entry, sizeof(entry), "|%s,%04o,%ld", bname, (unsigned)mode, sz);
        strncat(org, entry, max_org - strlen(org) - 1);
    }
    /* Kapanis ayirici */
    strncat(org, "|", max_org - strlen(org) - 1);

    size_t org_len = strlen(org);

    /* --- Arsiv dosyasini yaz --- */
    FILE *out = fopen(output, "wb");
    if (!out) {
        fprintf(stderr, "Hata: %s dosyasi olusturulamadi!\n", output);
        free(org);
        return 1;
    }

    /* 10 baytlik baslik: organizasyon bolumunun uzunlugu (sifir dolgulu) */
    fprintf(out, "%010lu", (unsigned long)org_len);

    /* Organizasyon bolumu */
    fwrite(org, 1, org_len, out);
    free(org);

    /* Dosya icerikleri art arda, ayirici olmadan */
    for (int i = 0; i < file_count; i++) {
        FILE *fp = fopen(files[i], "rb");
        if (!fp) {
            fprintf(stderr, "Hata: %s dosyasi okunamadi!\n", files[i]);
            fclose(out);
            return 1;
        }

        char   buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
            fwrite(buf, 1, n, out);

        fclose(fp);
    }

    fclose(out);
    printf("Arsiv dosyasi '%s' basariyla olusturuldu.\n", output);
    return 0;
}

/* ================================================================== */
/*  tarsau -a : Arsivi ac (extract)                                   */
/* ================================================================== */
static int do_extract(int argc, char *argv[])
{
    /*
     * -a parametresinden sonra en fazla 2 parametre:
     *   argv[2] = arsiv dosyasi (*.sau)
     *   argv[3] = [dizin]  (opsiyonel; yoksa gecerli dizin kullanilir)
     */
    if (argc < 3) {
        fprintf(stderr, "Kullanim: tarsau -a <arsiv.sau> [dizin]\n");
        return 1;
    }

    /* -a'dan sonra en fazla 2 parametre kontrolu (argv[4] ve sonrasi yoksayilir
       ama uyari verilir) */
    if (argc > 4) {
        fprintf(stderr,
                "Uyari: -a parametresinden sonra fazla arguman girildi; "
                "ilk ikisi kullanilacak.\n");
    }

    const char *archive = argv[2];
    const char *dir     = (argc >= 4) ? argv[3] : ".";

    /* .sau uzanti kontrolu */
    size_t alen = strlen(archive);
    if (alen < 5 || strcmp(archive + alen - 4, ".sau") != 0) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    /* Arsiv dosyasini ac */
    FILE *fp = fopen(archive, "rb");
    if (!fp) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    /* --- 10 baytlik basligi oku --- */
    char header[ORG_HEADER_LEN + 1];
    if (fread(header, 1, ORG_HEADER_LEN, fp) != (size_t)ORG_HEADER_LEN) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(fp);
        return 1;
    }
    header[ORG_HEADER_LEN] = '\0';

    /* Baslikta sadece rakam olmali */
    for (int i = 0; i < ORG_HEADER_LEN; i++) {
        if (header[i] < '0' || header[i] > '9') {
            fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            fclose(fp);
            return 1;
        }
    }

    long org_size = atol(header);
    if (org_size <= 0) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(fp);
        return 1;
    }

    /* --- Organizasyon bolumunu oku --- */
    char *org = (char *)malloc((size_t)org_size + 1);
    if (!org) {
        perror("malloc");
        fclose(fp);
        return 1;
    }

    if ((long)fread(org, 1, (size_t)org_size, fp) != org_size) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(org);
        fclose(fp);
        return 1;
    }
    org[org_size] = '\0';

    /* --- Organizasyon bolumunu ayristir --- */
    typedef struct {
        char   name[NAME_MAX + 1];
        mode_t mode;
        long   size;
    } FileEntry;

    FileEntry entries[MAX_FILES];
    int       entry_count = 0;

    /*
     * Format: |ad1,izin1,boyut1|ad2,izin2,boyut2|
     *
     * '|' ile ayrilmis her kaydi ayristir.
     */
    char *token = org;
    while (*token && entry_count < MAX_FILES) {
        /* Bastaki '|' karakterini atla */
        while (*token == '|')
            token++;
        if (*token == '\0')
            break;

        /* Bir sonraki '|' konumunu bul */
        char *end = strchr(token, '|');
        if (!end)
            break;

        /* Kaydi gecici tampona kopyala */
        size_t rec_len = (size_t)(end - token);
        char   record[NAME_MAX + 40];
        if (rec_len >= sizeof(record)) {
            fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(org);
            fclose(fp);
            return 1;
        }
        memcpy(record, token, rec_len);
        record[rec_len] = '\0';

        /* Kayit alanlari: ad,izin,boyut */
        char *c1 = strchr(record, ',');
        if (!c1) { token = end; continue; }
        *c1 = '\0';

        char *c2 = strchr(c1 + 1, ',');
        if (!c2) { token = end; continue; }
        *c2 = '\0';

        strncpy(entries[entry_count].name, record,
                sizeof(entries[entry_count].name) - 1);
        entries[entry_count].name[sizeof(entries[entry_count].name) - 1] = '\0';
        entries[entry_count].mode = (mode_t)strtol(c1 + 1, NULL, 8);
        entries[entry_count].size = atol(c2 + 1);
        entry_count++;

        token = end;   /* bir sonraki '|' e ilerle */
    }

    if (entry_count == 0) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(org);
        fclose(fp);
        return 1;
    }

    /* --- Cikis dizinini olustur --- */
    if (strcmp(dir, ".") != 0) {
        if (mkdirs(dir) != 0) {
            fprintf(stderr, "Hata: '%s' dizini olusturulamadi!\n", dir);
            free(org);
            fclose(fp);
            return 1;
        }
    }

    /* --- Dosyalari cikar --- */
    for (int i = 0; i < entry_count; i++) {
        /*
         * Hedef yolu olustur:
         *   dir == "."  =>  dosya_adi
         *   dir != "."  =>  dir/dosya_adi
         *
         * entries[i].name sadece basename icerdiginden path birlesimi dogrudur.
         */
        char filepath[PATH_MAX];
        if (strcmp(dir, ".") == 0) {
            snprintf(filepath, sizeof(filepath), "%s", entries[i].name);
        } else {
            snprintf(filepath, sizeof(filepath), "%s/%s", dir, entries[i].name);
        }

        /* Dosya icinde dizin bilesenlerini olustur (olasi alt dizinler) */
        {
            char dir_part[PATH_MAX];
            strncpy(dir_part, filepath, sizeof(dir_part) - 1);
            dir_part[sizeof(dir_part) - 1] = '\0';
            char *slash = strrchr(dir_part, '/');
            if (slash && slash != dir_part) {
                *slash = '\0';
                mkdirs(dir_part);
            }
        }

        /* Dosyayi yaz */
        FILE *outfp = fopen(filepath, "wb");
        if (!outfp) {
            fprintf(stderr, "Hata: %s dosyasi olusturulamadi!\n", filepath);
            free(org);
            fclose(fp);
            return 1;
        }

        long remaining = entries[i].size;
        char buf[8192];
        while (remaining > 0) {
            size_t to_read = ((long)sizeof(buf) < remaining)
                             ? sizeof(buf)
                             : (size_t)remaining;
            size_t n = fread(buf, 1, to_read, fp);
            if (n == 0) {
                fprintf(stderr,
                        "Uyari: %s dosyasi beklenen boyutta degil!\n",
                        entries[i].name);
                break;
            }
            fwrite(buf, 1, n, outfp);
            remaining -= (long)n;
        }

        fclose(outfp);

        /* Orijinal dosya izinlerini geri yukle */
        if (chmod(filepath, entries[i].mode) != 0) {
            fprintf(stderr,
                    "Uyari: %s icin izinler ayarlanamadi.\n", filepath);
        }
    }

    free(org);
    fclose(fp);

    /* Basari mesaji */
    printf("Arsiv '%s' basariyla acildi", archive);
    if (strcmp(dir, ".") != 0)
        printf(" -> %s", dir);
    printf("\n");

    printf("Cikarilan dosyalar (%d):\n", entry_count);
    for (int i = 0; i < entry_count; i++) {
        printf("  %s  (%ld bayt, izin: %04o)\n",
               entries[i].name,
               entries[i].size,
               (unsigned)entries[i].mode);
    }

    return 0;
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "tarsau - Sikistirmasiz Arsivleme Programi\n\n");
        fprintf(stderr, "Kullanim:\n");
        fprintf(stderr, "  tarsau -b dosya1 dosya2 ... [-o cikti.sau]\n");
        fprintf(stderr, "  tarsau -a arsiv.sau [dizin]\n\n");
        fprintf(stderr, "Secenekler:\n");
        fprintf(stderr, "  -b         Dosyalari arsivle (bundle)\n");
        fprintf(stderr, "  -a         Arsivi ac (extract)\n");
        fprintf(stderr, "  -o dosya   Cikti dosya adi (varsayilan: a.sau)\n");
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        return do_bundle(argc, argv);
    } else if (strcmp(argv[1], "-a") == 0) {
        return do_extract(argc, argv);
    } else {
        fprintf(stderr, "Hata: Gecersiz parametre '%s'\n", argv[1]);
        fprintf(stderr, "Kullanim:\n");
        fprintf(stderr, "  tarsau -b dosya1 dosya2 ... [-o cikti.sau]\n");
        fprintf(stderr, "  tarsau -a arsiv.sau [dizin]\n");
        return 1;
    }
}
