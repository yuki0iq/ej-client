# ej-client - an open source console client for ejudge

ej-client is a Qt-based console client for submitting solutions and viewing problems for ejudge

Don't know how to use this program? Try writing `help`!

ej-client was created in 2020 by Yuki0iq

# How to build

## Dependencies
* Any C++11 compliant compiler, but may compile with other
* qmake, QtCore and QtNetwork 5.10+, but may compile with other versions not lesser than 5.0
* make utility for making qmake-generated makefiles

## From command line
assuming `make` is your make utility
```
qmake ej-client.pro
make
```

## From Qt Creator
Open `ej-client.pro` in Qt Creator and press `Build` button
