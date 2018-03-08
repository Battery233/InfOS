#!/bin/sh

TOPDIR=`pwd`
CWKDIR=$TOPDIR/coursework
INFOS_REPO=$TOPDIR/infos
INFOS_USER_REPO=$TOPDIR/infos-user

if [ ! -d $CWKDIR ]
  then
    echo "  ERROR: COURSEWORK DIRECTORY DOES NOT EXIST"
    exit 1
fi

if [ ! -d $INFOS_REPO ]
  then
    echo "  ERROR: INFOS DIRECTORY DOES NOT EXIST"
    exit 1
fi

if [ ! -d $INFOS_USER_REPO ]
  then
    echo "  ERROR: INFOS USER DIRECTORY DOES NOT EXIST"
    exit 1
fi

echo "Cleaning InfOS repository..."
cd $INFOS_REPO
git clean -dfx
git reset --hard
git pull

echo
echo "Cleaning InfOS userspace repository..."
cd $INFOS_USER_REPO
git clean -dfx
git reset --hard
git pull

echo
echo "Restoring coursework link..."
ln -s $CWKDIR $INFOS_REPO/oot

echo
echo "DONE"
echo

