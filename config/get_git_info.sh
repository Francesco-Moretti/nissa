#!/bin/bash

echo '#ifndef _GIT_INFO'
echo '#define _GIT_INFO'

echo -n ' #define GIT_HASH "'
git rev-parse HEAD|tr -d "\n"
echo '"'

echo -n ' #define GIT_TIME "'
git log -1 --pretty=%ad|tr -d "\n"
echo '"'

echo -n ' #define GIT_LOG "'
git log -1 --pretty=%B|tr -d "\n"
echo '"'

echo '#endif'