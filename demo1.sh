date +%s%N
for i in {1..20}
do
# cp pics/logo_inst210.gif temp/logo_$((i)).gif # step 1, 2, 3
# cp pics/logo_inst210.gif temp/logo_$((i)).gif+private # step 4a
# cp temp/2012/December/logo_$((i)).gif+private temp/logo_$((i)).gif # step 4b
# cp pics/Cherry002.jpg temp/Cherry002_$((i)).jpg # step 5, 6
# cp pics/Cherry002.jpg temp/Cherry002_$((i)).jpg+private #step 7a
# cp temp/2012/December/Cherry002_$((i)).jpg+private temp/Cherry002_$((i)).jpg #step 7b

done
date +%s%N
