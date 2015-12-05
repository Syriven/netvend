//sending/receiving addresses

unsigned char buf[MAX_ADDRESS_SIZE];
memset(buf, '\0', MAX_ADDRESS_SIZE);//this way our address will be padded if size < 34

depositAddress_.copy((char*)buf, depositAddress_.size());

networking::writeBufOrThrow(socket, buf, MAX_ADDRESS_SIZE);

//leave an extra byte so even a full address is followed by a \0.
//this allows the string(buf) constructor later to get the right size

unsigned char buf[MAX_ADDRESS_SIZE+1];
memset(buf, '\0', MAX_ADDRESS_SIZE+1);

networking::readToBufOrThrow(socket, buf, MAX_ADDRESS_SIZE);

std::string depositAddress((char*)buf);//this will eliminate trailing '\0's for addresses < 34 size


//             CommandQueue cq;
//             cq.setID(9);
//             cq.addHandshakeCommand(pubkey);
//             cq.addHandshakeCommand(pubkey);
//             std::vector<unsigned char> encodedCQ = cq.encode();
// 
//             //verify
//             unsigned char n;
//             unsigned long int id;
//             int place=0;
//             place += unpack(encodedCQ.data(), "LC", &id, &n);
//             std::cout << id << " " << (int)n << std::endl;
//             for (int i=0;i<n;i++) {
    //                     unsigned char type;
//                     unsigned long int cmdSize;
//                     place += unpack(encodedCQ.data()+place, "CL", &type, &cmdSize);
//                     std::cout << type << " " << cmdSize << std::endl;
// 
//                     CryptoPP::ByteQueue bq;
//                     bq.Put(encodedCQ.data()+place, cmdSize);
//                     place += cmdSize;
//                     crypto::RSAPubkey pk;
//                     bq.MessageEnd();
//                     pk.Load(bq);
//                     std::cout << RSAPubkeyToNetvendAddress(pk) << std::endl;
//             }


void createPost(std::string name, std::vector<unsigned char> dataVch) {
    connectToNetvend();
    
    //create commandBatch with single createPost command
    boost::shared_ptr<commands::Command> command(new commands::CreatePostCommand(name, dataVch));
    
    commands::CommandBatch* cb = new commands::CommandBatch();
    cb->addCommand(command);
    
    //get encoded commandBatch
    boost::shared_ptr<std::vector<unsigned char> > cbData(new std::vector<unsigned char>());
    cb->writeToVch(cbData.get());
    
    std::vector<unsigned char> sig = crypto::cryptoSign(*cbData, privkey, rng);
    
    //create commandBatchPacket
    networking::CommandBatchPacket cbp(agentAddress, cbData, sig);
    
    //send it
    cbp.writeToSocket(nvConnection->socket());
    
    //receive response
    boost::shared_ptr<networking::CommandBatchResponse> response(networking::CommandBatchResponse::readFromSocket(nvConnection->socket(), cb));
    if (response.get() == NULL) std::cerr << "read failed" << std::endl;
    
    disconnectFromNetvend();
    
    std::cout << response->commandResultBatch()->commandResults()->size() << std::endl;
}

void createPost(std::string name, std::string dataStr) {
    std::vector<unsigned char> vch(dataStr.size());
    copy(dataStr.begin(), dataStr.end(), vch.data());
    createPost(name, vch);
}