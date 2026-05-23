# tarsau - Sikistirmasiz Arsivleme Programi
# Sistem Programlama 2025-2026 Bahar Donemi Projesi

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
	@rm -rf test_output test1.txt test2.txt test3.txt binary.bin
	@echo "Temizlendi."

# -------------------------------------------------------------------
# Test hedefi
# -------------------------------------------------------------------
test: derle bagla
	@echo "============================================"
	@echo " Test 1: Ornek metin dosyalari olusturma"
	@echo "============================================"
	@printf "Merhaba Dunya!\nBu bir test dosyasidir.\nSatir 3.\n" > test1.txt
	@printf "Ikinci test dosyasinin icerigi.\nSatir 2.\n" > test2.txt
	@printf "Ucuncu ve son test dosyasi.\n" > test3.txt
	@echo "Dosyalar olusturuldu: test1.txt test2.txt test3.txt"
	@echo ""

	@echo "============================================"
	@echo " Test 2: Arsivleme (-b)"
	@echo "============================================"
	./bin/$(TARGET) -b test1.txt test2.txt test3.txt -o test.sau
	@echo ""

	@echo "=== Arsiv dosyasi ham icerigi ==="
	@cat test.sau
	@echo ""
	@echo ""

	@echo "============================================"
	@echo " Test 3: Arsiv acma (-a) -> test_output/"
	@echo "============================================"
	@mkdir -p test_output
	./bin/$(TARGET) -a test.sau test_output
	@echo ""

	@echo "============================================"
	@echo " Test 4: Icerik dogrulama (diff)"
	@echo "============================================"
	@diff test1.txt test_output/test1.txt && echo "  test1.txt : BASARILI"
	@diff test2.txt test_output/test2.txt && echo "  test2.txt : BASARILI"
	@diff test3.txt test_output/test3.txt && echo "  test3.txt : BASARILI"
	@echo ""

	@echo "============================================"
	@echo " Test 5: Varsayilan cikti (a.sau)"
	@echo "============================================"
	./bin/$(TARGET) -b test1.txt test2.txt
	@test -f a.sau && echo "  a.sau olusturuldu: BASARILI"
	@rm -f a.sau
	@echo ""

	@echo "============================================"
	@echo " Test 6: Binary dosya reddi"
	@echo "============================================"
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0,1,2,3,255]))" > binary.bin
	@./bin/$(TARGET) -b binary.bin 2>&1 | grep -q "uyumsuzdur" && \
		echo "  Binary dosya reddedildi: BASARILI" || \
		echo "  HATA: Binary dosya kabul edilmemeli!"
	@echo ""

	@echo "============================================"
	@echo " Test 7: Gecersiz arsiv acma"
	@echo "============================================"
	@./bin/$(TARGET) -a gecersiz_dosya.sau 2>&1 | grep -q "uygunsuz" && \
		echo "  Gecersiz arsiv reddedildi: BASARILI" || \
		echo "  HATA: Hata mesaji bekleniyor!"
	@echo ""

	@echo "============================================"
	@echo " Tum testler tamamlandi!"
	@echo "============================================"
	@rm -rf test1.txt test2.txt test3.txt test.sau test_output binary.bin
