# Results and Conclusions:  We tested on different files from 10kb to 646 MB in our V6 File system




# main() function description- It will take the commands in command line and proceed further according to the commands given further in command line!
# The functions used are as follows:-
        The Functions which are implemented and are used in command line arguments:-
                - initfs() - Initiale the V6 file system
                - ls():- Display the contents of the current directoy
                - makeDirectory():- Create a directory
                - changeDirectory():- Change the current working directory specified in the command line.
                - copyIn():- Copy the contents of the external file into a new file in V6file system.
                - copyOut():- Copy the contents back to the newly created external file from the file in V6file system                  which was created earlier.
                - removeFile():- remove the file from the V6 File system which as created earlier
                - removeDirectory():- Remove the specified directory given in command line
                - openFileSystem():- Open the External file which is in V6 FIle system mode
                - quit() :- It will save and quit all the changes
        The Functions which are getting utilized in calling above functions:-
                - findLastIndex()
                - sliceString()
                - addAFreeBlock()
                - getAFreeBlock()
                - addAFreeInode()
                - getAnInode()
                - getAFreeInode()
                - writeTheInode()
                - initializeRootDirectory()
      

# Names of the Input File for copyIn() -
        test.txt

       

# How to use the code:
        ## Compile :
                - gcc fileSystem.c
        ## Run :
                - ./a.out 


# Error Checking:
        ##  If any of the arguments from V6 filesystem name, total number of blocks, total number of inodes are missing   Display: Insufficient arguments, cannot proceed.
        ## If while opening the file the file is unable to get opened or filepath is not found, 
        Display: Error while opening the file.

 
