target:=test

$(target):test.cpp
	g++ -Wall -std=c++17 $< -o $@

clean:
	rm -f $(target)