#!/bin/sh

echo "Checking project"
if [ project.pbxproj.datecheck -nt ../UE4_FromPC.xcodeproj/project.pbxproj ]
then
  echo "Copying project..."
  mkdir -p ../UE4_FromPC.xcodeproj > /dev/null
  cp project.pbxproj.datecheck ../UE4_FromPC.xcodeproj/project.pbxproj
else
  echo "Project file is up to date."
fi

echo "Checking for inline breakpoint strategy"
if [ -n "ls | grep 'strategy' ~/.lldbinit" ]
then
  echo "Strategy found... inline breakpoints already enabled"
else
  echo "settings set target.inline-breakpoint-strategy always" >> ~/.lldbinit
  echo "Strategy not found -> enabling all inline breakpoints"
fi

if [ -e ../../../../$1/Binaries/IOS/$1-$2-$3$4.app.dSYM.zip.datecheck ]
then
  pushd ../../../../$1/Binaries/IOS > /dev/null
  if [ $1-$2-$3$4.app.dSYM.zip.datecheck -nt $1-$2-$3.app.dSYM.zip ]
  then
    echo "Unzipping new .dSYM..."
    mv $1-$2-$3$4.app.dSYM.zip.datecheck $1-$2-$3$4.app.dSYM.zip
    unzip -o $1-$2-$3$4.app.dSYM.zip
  else
    echo ".dSYM is up to date."
  fi
  popd > /dev/null
else
  echo $1"-"$2"-"$3""$4".app.dSYM not found..."
fi

if [ -e ../../../../$1/Binaries/IOS/$1-$2-$3$4.app.dSYM ]
then
  pushd ../../../../$1/Binaries/IOS > /dev/null
  cp -R $1-$2-$3$4.app.dSYM Payload/$1.app.dSYM
  popd > /dev/null
fi

if [ -e ../../../../$1/Binaries/IOS/$1$4.app.dSYM ]
then
  pushd ../../../../$1/Binaries/IOS > /dev/null
  cp -R $1$4.app.dSYM Payload/$1$4.app.dSYM
  popd > /dev/null
fi
