# netvend

Netvend is a fresh-start re-imagining of the idea behind the first netvend prototype, [netvend-proto](https://github.com/Syriven/netvend-proto).

Netvend is a microservice server which executes cryptographically signed commands from pseudonymous agents. These agents must have deposited Bitcoin to fund their accounts to have access to most services. On each command the agent executes, the server verifies the signature, deducts a small fee, and returns any results to the agent in a low-level api designed to be as simple and dynamic as possible.

The microservices netvend offers are focused on marketplace, networking, and broadcasting actions. The utility netvend offers is comparable to that of a local farmer's market: the server acts as a common-grounds, neutral meeting place for agents who want to buy, sell, and communicate with one another, but have no common, trustless meeting place to find one another. If Bitcoin is the money of the Internet, netvend aims to be the marketplace.

Netvend is designed primarily to handle script-based agents, which may or may not be interfacing with a human for essential input. As long as an agent has funds to cover the cost of the requested commands, there are no limits or caps on any activity.

Because netvend does not discriminate between "spam" and "legitimate" content, it is the agent's responsibility to download responsibly. However, because all agents post under a public key, netvend and its agents can more securely and easily track reputation.

Two agents that communicate may be:
- Two copies of the same code, doing normal networking actions; i.e. multiplayer data
- Two different programs, whose developers are actively collaborating on a common project
- Two different programs, whose developers are not actively collaborating

# Credits

pack.cpp is a slightly adapted bit of code found [here](http://cboard.cprogramming.com/c-programming/80761-writing-pack-style-function-any-advices.html), written by the user isaac_s.

b58check.cpp was written with a ton of reference to [bitcoin's base58.cpp](https://github.com/bitcoin/bitcoin/blob/master/src/base58.cpp).
