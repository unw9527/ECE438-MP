git checkout _grades -q
git pull
echo
echo '-----------Contents of results.txt-----------------'
echo
cat results.txt
echo
echo '-----------End of results.txt----------------------'
echo
git checkout master -q
