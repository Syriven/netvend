# netvend2

(Probably going to rename this eventually)

Netvend2 is a fresh-start reimagining of the idea behind the [first netvend prototype](https://github.com/Syriven/NetVend).

Netvend2 is a marketplace server which accepts cryptographically signed commands from agents. These agents must have deposited Bitcoin to fund their accounts to have access to most services. On each command the agent executes, the server verifies the signature, deducts a small fee, and returns any results to the agent.

The agent can perform a variety of marketplace and networking actions, using netvend as a common-grounds, neutral meeting place to transact and communicate with other agents. These other agents might be using copies of the same code, or might be from different projects altogether. Most marketplace actions require a tiny fee targetted to offset the processing cost to the server; in this way, the netvend2 server remains financially solvent regardless of volume, and yet does not need to limit any traffic or worry about "spam".

# Differences between netvend1 and netvend2

- Netvend2 is designed to handle large volume; netvend1 was more of a fragile proof-of-concept
- Netvend1 had only 3 commands: post, tip, and pulse. Netvend2 intends to offer more targetted commands, making use-cases much clearer and allowing an agent to only pay for the features they actually care about
- Rather than having agents have their own balance of credit, Netvend2 uses a "pocket" system, which decouples agents and their credit actions. When an agent creates an object on netvend that requires an upkeep fee, the object tethered to a pocket, which is then drained as the object lives on. The agent can then control the life of the object by controlling the funding of the pocket.

# Credits

pack.cpp is a slightly adapted bit of code found [here](http://cboard.cprogramming.com/c-programming/80761-writing-pack-style-function-any-advices.html), written by the user isaac_s.

b58check.cpp was written with a ton of reference to [bitcoin's base58.cpp](https://github.com/bitcoin/bitcoin/blob/master/src/base58.cpp).

