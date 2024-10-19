# Makefiles
two seperate make files are there for both tracker and client , we need to run them both before starting

# Tracker
only one tracker was implemented, which stores filename,ip,piecewise,filesize  information from clients, groups were not implemented,
but login and create_user were implemented

# client

whenever download is requested the file gathers piecewise information of file and ips from tracker from there, each piece is requested iin 
seprate thread which downloads 16 subpieces of 32kb separately from each peer that is present, the missing_pieces array is shuffled for
randomization , even if one piece download is finished the client updates its status in tracker 
