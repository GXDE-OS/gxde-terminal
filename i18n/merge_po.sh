#!/bin/sh
for lang in ../po/*
do
    mv ../po/$(basename ${lang})/LC_MESSAGES/gxde-terminal.po ../po/$(basename ${lang})/LC_MESSAGES/gxde-terminal_backup.po     
    msgmerge ../po/$(basename ${lang})/LC_MESSAGES/gxde-terminal_backup.po gxde-terminal.pot > ../po/$(basename ${lang})/LC_MESSAGES/gxde-terminal.po
    rm ../po/$(basename ${lang})/LC_MESSAGES/gxde-terminal_backup.po     
done    

