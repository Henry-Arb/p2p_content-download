# what's done:
- content registration
- content de-registration
- quitting functionality (with automatic de-registration)
- tcp socket creation and sending over associated port # to index server
- how to use `select()` in TCP download implementation
- how to actually `listen()` to the tcp sockets that are opened
- peer-request content download feature
- peer-provide content download feature

# TO DO:
- register peer to index server upon succesful download completion
- optional: implement complimentary "cancel" function (example: cancel registration process, cancel download process, etc.)
- optional: refactor for memcpy vs sprintf vs strncpy usage consistency
- optional: list local files of peer (so we can check if it's really something we have before register, without needing to ctrl+C to view local files)