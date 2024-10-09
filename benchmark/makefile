target1:=simplerTaskTest
target2:=complexTaskTest

all:$(target1) $(target2)

$(target1):simplerTaskTest.cpp
	g++ -Wall -std=c++17 $< -o $@

$(target2):complexTaskTest.cpp
	g++ -Wall -std=c++17 $< -o $@