.PHONY: clean all

CFLAGS += -g
CFLAGS += -O3
# CFLAGS += -fsanitize=address

all: basic fast cdcl loop
fast: fast.c
basic: basic.c
cdcl: cdcl.c
loop: loop.c

clean:
	rm -f basic fast loop cdcl xcheck_*.output perf.*

ALL_FILES = $(wildcard ../test_inputs/*.dimacs) $(wildcard ../test_inputs/*.cnf)

BASIC_FILES = \
	../test_inputs/aim-50.dimacs \
	../test_inputs/dubois22.dimacs \
	../test_inputs/p2.dimacs \
	../test_inputs/p5.dimacs \
	../test_inputs/p7.dimacs \
	../test_inputs/sat.dimacs \
	../test_inputs/test.dimacs

FAST_FILES = $(filter-out \
	../test_inputs/aim-100.dimacs \
	../test_inputs/big.dimacs \
	$(wildcard ../test_inputs/*.cnf), \
	$(ALL_FILES))

CDCL_FILES = $(filter-out ../test_inputs/aim-50.dimacs,$(ALL_FILES))


check-basic: basic
	@for file in $(BASIC_FILES); do \
		echo "Running $$file..."; \
		./basic < $$file; \
		case $$file in \
			*aim-50*)    echo "=> SAT" ;; \
			*aim-100*)   echo "=> UNSAT" ;; \
			*dubois22*)  echo "=> UNSAT" ;; \
			*sat.dimacs) echo "=> SAT" ;; \
			*test.dimacs)echo "=> SAT" ;; \
			*big.dimacs) echo "=> UNSAT" ;; \
			*)           echo "=> UNSAT" ;; \
		esac; \
		echo ""; \
	done

check-fast: fast
	@for file in $(FAST_FILES); do \
		echo "Running $$file..."; \
		./fast < $$file; \
		case $$file in \
			*aim-50*)    echo "=> SAT" ;; \
			*aim-100*)   echo "=> UNSAT" ;; \
			*dubois22*)  echo "=> UNSAT" ;; \
			*sat.dimacs) echo "=> SAT" ;; \
			*test.dimacs)echo "=> SAT" ;; \
			*big.dimacs) echo "=> UNSAT" ;; \
			*)           echo "=> UNSAT" ;; \
		esac; \
		echo ""; \
	done

check-cdcl: cdcl
	@for file in $(CDCL_FILES); do \
		echo "Running $$file..."; \
		./cdcl < $$file; \
		case $$file in \
			*aim-50*)    echo "=> SAT" ;; \
			*aim-100*)   echo "=> UNSAT" ;; \
			*dubois22*)  echo "=> UNSAT" ;; \
			*sat.dimacs) echo "=> SAT" ;; \
			*test.dimacs)echo "=> SAT" ;; \
			*big.dimacs) echo "=> UNSAT" ;; \
			*)           echo "=> UNSAT" ;; \
		esac; \
		echo ""; \
	done