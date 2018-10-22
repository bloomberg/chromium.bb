Strawberry Perl Portable 5.16.0.1-32bit README
==============================================

What is Strawberry Perl Portable?
---------------------------------

* 'Perl' is a programming language suitable for writing simple scripts as well
  as complex applications. See http://perldoc.perl.org/perlintro.html

* 'Strawberry Perl' is a perl environment for Microsoft Windows containing all
  you need to run and develop perl applications. It is designed to be as close
  as possible to perl environment on UNIX systems. See http://strawberryperl.com/

* 'Strawberry Perl Portable' is Strawberry Perl that you do not need to install,
  you do not need admin privileges, you just extract provided ZIP on your disk.
 
How to use Strawberry Perl Portable?
------------------------------------

* Extract strawberry portable ZIP into e.g. c:\myperl\ 
  Note: choose a directory name without spaces and non us-ascii characters

* Launch c:\myperl\portableshell.bat - it should open a command prompt window

* In the command prompt window you can:

  1. run any perl script by launching
  
     c:\> perl c:\path\to\script.pl

  2. install additional perl modules (libraries) from http://www.cpan.org/ by

     c:\> cpan Module::Name
  
  3. run other tools included in strawberry like: perldoc, gcc, dmake ...

* If you want to use Strawberry Perl Portable not only from portableshell.bat,
  you need to set the following environmental variables:
  
  1. add c:\myperl\perl\site\bin, c:\myperl\perl\bin, and c:\myperl\c\bin 
     to PATH variable
  
  2. set variable TERM=dumb
