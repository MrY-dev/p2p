# Makefiles
two seperate make files are there for both tracker and client , we need to run them both before starting

# Tracker
Tracker keeps track of the pieces of information that every peer holds, whenever a request is made by client, the information about the peers is
sent to it directly.

# client
whenever download is requested the file gathers piecewise information of file and ips from tracker from there, each piece is requested in 
seprate thread which downloads 16 subpieces of 32kb separately from each peer that is present, the missing_pieces array is shuffled for
randomization , even if one piece download is finished the client updates its status in tracker 
