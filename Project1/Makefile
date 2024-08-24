files = Makefile README hw1.c

#Default to build http-get executable when make is called
default:
	gcc hw1.c -o http-get

#To remove the http-get executable when make clean is called
clean:
	rm -f http-get

#To perform the tests when make test is called
test: http-get
	@echo "Testing -a and -c with http://ucla.edu"
	./http-get -a  http://ucla.edu -c
	@echo "Testing -a with http://ucla.edu"
	./http-get -a  http://ucla.edu

dist:
	tar -czf 105180929.tar.gz $(files)
