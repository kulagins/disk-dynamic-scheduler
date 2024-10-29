
#COVERAGE = --coverage

CC = gcc

# In CFLAGS, make sure to add the path to igraph and graphviz headers
CFLAGS = -O0 -g -Wall -pedantic -DIL_STD -Wno-long-long -Wno-deprecated -Wno-unused-variable -Wno-deprecated-declarations -I/usr/local/include -I/opt/local/include $(COVERAGE)

# LDFLAGS should also include the path to igraph and graphviz library
LDFLAGS = -L/opt/local/lib -L/usr/local/lib  $(COVERAGE) 
LIBS = -lcgraph -lcdt -ligraph -lm -lgvc
TEST_SRC = $(shell find ./ -name "test-*c")
TARGETS  = $(TEST_SRC:.c=) memdag
TARGETS_OBJ = $(TARGETS:%=%.o)
SRC     = $(shell find ./ -name "*.c" |grep -v "memdag\.c" | grep -v "test-.*\.c" | grep -v "old\.c" | grep -v "new\.c" | grep -v "\._*\.c" )
HEADERS = $(shell find ./ -name "*.h" | grep -v "\-old\.")
OBJ     = $(SRC:.c=.o)
OBJDIR  = .

all: $(TARGETS) 

$(TARGETS): $(OBJ) $(TARGETS_OBJ)
	@echo "Linking $@"
#	@echo "SRC is: " $(SRC)
#	@echo "TARGETS_OBJ is " $(TARGETS_OBJ)
	$(CC) $(LDFLAGS) -o  $@ $(OBJ) $@.o $(LIBS)


$(OBJDIR)/%.o: %.c $(HEADERS)
	@echo "Compiling $@"
	$(CC) $(CFLAGS)  -c $< -o $@

destroy: clean
	@rm -f $(TARGETS)

rebuild: destroy all


%.pdf: %.dot
	dot -Tdf $< -o $@

tests: $(TARGETS)
	./test-script.sh

cov: 
	make COVERAGE=--coverage clean tests
	lcov --capture --directory ./ --output-file coverage.info
	genhtml coverage.info --output-directory ../html-cov/
	make destroy

doc:
	doxygen Doxyfile

clean: 
	@rm -f *.o *.gcda *.gcno coverage.info tmp.sp.dot 
	@rm -f $(TARGET)
	@rm -rf *.dSYM

.PHONY: all clean tests cov destroy rebuild
