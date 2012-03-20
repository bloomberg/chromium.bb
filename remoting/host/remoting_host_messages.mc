SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR)

FacilityNames=(Host=0x0:FACILITY_HOST)

LanguageNames=(English=0x409:MSG00409)

; // The categories of events.
MessageIdTypedef=WORD

MessageId=0x1
SymbolicName=HOST_CATEGORY
Language=English
Host
.


; // The message definitions.
MessageIdTypedef=DWORD

MessageId=1
Severity=Informational
Facility=Host
SymbolicName=MSG_HOST_CLIENT_CONNECTED
Language=English
Client connected: %1.
.

MessageId=2
Severity=Informational
Facility=Host
SymbolicName=MSG_HOST_CLIENT_DISCONNECTED
Language=English
Client disconnected: %1.
.

MessageId=3
Severity=Error
Facility=Host
SymbolicName=MSG_HOST_CLIENT_ACCESS_DENIED
Language=English
Access denied for client: %1.
.

MessageId=4
Severity=Informational
Facility=Host
SymbolicName=MSG_HOST_CLIENT_ROUTING_CHANGED
Language=English
Channel IP for client: %1 ip='%2' host_ip='%3' channel='%4' connection='%5'.
.
