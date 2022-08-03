#! /bin/bash

hostname -I | grep -oE "[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+" | head -1
