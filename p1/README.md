# my-look

First I check the number of arguments, if it is less than 1 or more than
4 the program will exit(-1) 

Then I check whether the file can be opened.

Next, I use an array of char to store the line read from the file and
use `strncasecmp` to compare the line and given prefix.

Finally close the file.

# across
First I check the number of arguments.

Then I check whether every letter of given word is lower case.(Because
the detail requests us to ignore the lines containing characters other
than lower cases.

Then I check whether the position and length are valid.

Then I start to check every line: 1. Whether contains non-lowercase 
letters. 2. Whether contains the given word at given position.

Finally close file.

# my-diff
First check the arguments number and whether can open files.

Then use two pointers to traverse all the lines of two files. For each
`getline`: if return values of `getline` are same and return value of 
`strcmp` is 0 then skip; otherwise print the line according to the 
return value of `getline`

What's more, use `prev` to record the last printed line.

Finally close two files.
