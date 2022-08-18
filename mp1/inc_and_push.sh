#!/bin/bash
typeset -i version=$(cat version.txt)

printf "\nchanging version from $version to $((version+1))\n\n"
version=$((version+1))
echo "$version" > version.txt

git add -u
git commit -m "incremented version to $version"
git push origin master
