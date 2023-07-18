#!/bin/sh

tr '\n' '\0' < install_manifest.txt | xargs -0 sudo rm  #some of the files have spaces
