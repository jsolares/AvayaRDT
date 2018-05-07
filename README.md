# AvayaRDT
Sample server for Avaya RDT capture

Using libevent and bstrlib.

[Brief description of the RDT Protocol as used by Avaya in RDTT](https://chema.solaresmaldonado.com/linux/13/the-rdt-protocol/)

This protocol was/is used for CDR purposes on Avaya equipment
This code serves as an example for capturing the CDR and extracting the phone extensions that finalized a call, you could modify it and remove the other client code for asterisk termination of calls and to store the CDR to a MySQL database.

Code is provided as is.
