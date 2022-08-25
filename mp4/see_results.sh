git checkout _grades -q
git pull -q
echo
echo '-----------Contents of results.txt-----------------'
echo
cat results.txt
echo
echo '-----------End of results.txt----------------------'
echo
git checkout main -q
