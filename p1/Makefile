All:
    gcc -g -Wall -Werror -o my-look my-look.c
    gcc -g -Wall -Werror -o across across.c
    gcc -g -Wall -Werror -o my-diff my-diff.c

my-look:
    gcc -g -Wall -Werror -o my-look my-look.c

across:
    gcc -g -Wall -Werror -o across across.c

my-diff:
    gcc -g -Wall -Werror -o my-diff my-diff.c

check:
    cpplint.py --extensions=c,h my-look.c
    cpplint.py --extensions=c,h across.c
    cpplint.py --extensions=c,h my-diff.c

clean:
    rm my-look
    rm across
    rm my-diff