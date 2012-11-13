cs3210_proj3
============

Your Picture File System

Keeping pictures organized is a persistent problem that many people have to face.  One approach is to provide application software that scans your hard drive in order to find all possible picture files and then either copy them into an application-specified location or to simply note where they are and generate metadata so the application can find them again.  Many such applications exist, both free and commercially available.

However, it is now clear that certain operations are "universal" from a user's perspective -- the ability to sort by the date the picture was taken, or by a geotagged location, or by a recognized face.  Additionally, there's a clear desire to be able to share pictures socially, but with some degree of privacy or protection imbedded.  In the finest tradition of Operating Systems design, for this assignment you are therefore going to take this new abstraction of a "picture file" and design a file system that reflects some of the distinctions in how a user wants to interact with pictures as opposed to other data that might reside in a file system.
YPFS Features

 This assignment is reasonably open-ended, and there is room for considerable innovation on your part.  The minimum requirements are listed below, but feel free to be creative and go above and beyond.

A sample of the expected directory structure is provided below:

./Dates

-->/2009

-->/2008

-->-->/January

-->-->/February

-->-->-->/Pic1.jpg

-->-->-->/Pic2.jpg


And the following should complete correctly:

cp ./Dates/2008/February/Pic1.jpg ${HOMEDIR}/newpics

At a minimum, here are some of the features that YPFS should be expected to support:

    Automatic Sorting.  Pictures should be only copied into the root level of the file system.  I.E.  If you have mounted it at ~/MyPFS in your home directory, you would just copy pictures to that directory, not any subdirectories.  YPFS should not display them there, though -- it should automatically move/link the file to the appriate subdirectories.
    Sorting by date taken.  For pictures that have EXIF headers, they should be read.  For other pictures, the date the file was created should be used instead. 
    Clean Back-end Storage.  You won't be implementing a full file system, so any files in ~/MyPFS will have to have a representation in a back-end store. Conceptually, this could be anything -- a database, a magic file in /tmp, a hidden ~/.mypics directory, or access to the "hidden" ~/MyPFS director.   Note that the pictures themselves don't even have to be there -- for instance, the pictures themselves could be on flickr, picasaweb, or twitter, and you just store URL and oath-associated data in your local store.  Whatever you design and implement should be clean and clearly articulated.
    Privacy.   Particularly for scenarios where pictures in the file system are shared across social media as well (or are on semi-shared devices), it is desirable to be able to flag certain pictures as being private.  A user should be able to append "+private" to the end of the name of a picture (for example, "mv Pic2.jpg PIc2.jpg+private"), and the file system will automatically encrypt the picture.  Key manangement for encryption should be maintained so that it is not exposed to non-qualified users, and also so that the file system can switch between a "locked" and an "unlocked" mode for everything that has been marked as private.

Tools

 You will be using the FUSE toolkit to implement your file system for this project.  Accessing complex libraries (like those needed to read the exif headers) from the kernel is problematic, and FUSE helps to solve this.  FUSE, in essence, brings a bit of microkernel to the Linux macrokernel.  It implements the Linux virtual file system interface in the kernel by providing callback functions to a registered userspace program.  The userspace daemon can then perform the action as requested and supply the updates to the inodes, directory structures, etc. through a set of provided functions.

You can go and dowload FUSE and build and install it for your 2.6.24 kernel if you like.  However, a package for the redhat kernel already exists and has been installed for your use on all of the factors.  There are a number of tutorials and "hello world" examples to read online about how to get started with fuse.  Useful pages to start with are

http://fuse.sourceforge.net/  -- The main page for all things fuse.

http://fuse.sourceforge.net/helloworld.html  -- a simple hello world

EXIF data is something that you can use pre-existing implementations to access.  For example, libexif (http://libexif.sourceforge.net/) or exiv2 (http://www.exiv2.org/) are both good solutions.

