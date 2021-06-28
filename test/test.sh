# Tworzenie dysku
../bin/mbfs-mkfs disk.img 100000000

LONG_NAME="Minim tempor labore deserunt nostrud et incididunt velit quis et nulla reprehenderit ullamco elit nisi. Occaecat quis Lorem culpa magna adipisicing aliqua consequat quis enim qui officia duis elit ullamco. Qui dolore sit cupidatat quis. Laborum non tempor quis aliquip consectetur magna est ipsum magna. Velit aute enim amet ullamco adipisicing velit quis excepteur elit.txt"

# Tworzenie plików
echo "Tworzenie plików..."
dd if=/dev/random of="-asdf.bin" bs=30000 count=1 2> /dev/null
dd if=/dev/random of="*asdf.bin" bs=40000 count=1 2> /dev/null
dd if=/dev/random of="ążźćół.bin" bs=50000 count=1 2> /dev/null
dd if=/dev/random of="xyz1.bin" bs=2000 count=1 2> /dev/null
dd if=/dev/random of="xyz2.bin" bs=2000 count=1 2> /dev/null
dd if=/dev/random of="xyz3.bin" bs=2000 count=1 2> /dev/null
dd if=/dev/random of="large1.bin" bs=50000000 count=1 2> /dev/null
dd if=/dev/random of="large2.bin" bs=70000000 count=1 2> /dev/null
echo ""

# Kopiowaniej małych plików
../bin/mbfs-cpd disk.img ążźćół.bin
../bin/mbfs-cpd disk.img "-asdf.bin" "*asdf.bin" 
../bin/mbfs-cpd disk.img xyz*
../bin/mbfs-cpd disk.img "$LONG_NAME"

# Wyświetlenie zawartości dysku
echo "Dysk z małymi plikami:"
../bin/mbfs-ls disk.img
echo ""

# Usuwanie kilku plików
../bin/mbfs-rm disk.img "-asdf.bin"
../bin/mbfs-rm disk.img "*asdf.bin"
../bin/mbfs-rm disk.img xyz1.c
../bin/mbfs-rm disk.img "$LONG_NAME"

# Wyświetlenie zawartości dysku
echo "Dysk po usunięciu czterech plików:"
../bin/mbfs-ls disk.img
echo ""

# Wyświetlenie struktury dysku
echo "Struktura dysku:"
../bin/mbfs-struct disk.img
echo ""

# Kopiowanie jednego pliku
../bin/mbfs-cpd disk.img "-asdf.bin"

# Wyświetlenie struktury dysku
echo "Struktura dysku po wgraniu pliku \"-asdf.bin\":"
../bin/mbfs-struct disk.img
echo ""

# Tworzenie drugiego dysku z końcową zawartością pierwszego
../bin/mbfs-mkfs disk2.img 1000000
../bin/mbfs-cpd disk2.img ążźćół.bin xyz2.bin xyz.3 ążźćół.bin "-asdf.bin"

echo "Struktura drugiego dysku po wgraniu zawartości pierwszego:"
../bin/mbfs-struct disk2.img
echo ""

# Kopiwanie dużych plików
echo "Kopiowanie dużych plików..."
../bin/mbfs-cpd disk.img large1.bin large2.bin

# Wyświetlenie zawartości dysku
echo "Dysk z dużymi plikami:"
../bin/mbfs-ls disk.img
echo ""

# Kopiowanie danych z dysku 
mkdir -p copy
../bin/mbfs-cph disk.img large1.bin large2.bin ążźćół.bin ./copy/

# Sprawdzenie integralności danych
md5 large1.bin ./copy/large1.bin
md5 large2.bin ./copy/large2.bin
md5 ążźćół.bin ./copy/ążźćół.bin

# Sprzątanie
rm -R copy
rm disk.img 
rm disk2.img 
rm *.bin

