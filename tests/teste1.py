import sys, os
from os.path import expanduser

home = expanduser("~")

os.system("mkdir " + home + "/teste")
os.system("rm -r " + home + "/sync_dir_*")
os.system("rm -r " + home + "/server_sync_dir_*")
os.system("rm -r " + home + "/teste/*")

os.chdir("..")
os.system("make");

os.system("./bin/dropboxServer " + str(sys.argv[1]) )
#os.system("HOME=/home/grad/fmmazzola/teste ./bin/dropboxClient" + str(sys.argv[1]) + "&")
