CREATE TABLE agents (
    agent_address character(34) NOT NULL,
    public_key bytea NOT NULL,
    default_pocket INT NOT NULL,
    PRIMARY KEY (agent_address)
);

CREATE TABLE pockets (
    pocket_id SERIAL,
    owner character(34) REFERENCES agents(agent_address),
    amount int DEFAULT(0),
    deposit_address character(34),
    PRIMARY KEY (pocket_id)
);

ALTER TABLE agents ADD FOREIGN KEY (default_pocket) REFERENCES pockets(pocket_id);