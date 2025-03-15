# D-Star Service Gateway

> [!CAUTION]
> For now, this repo is a scratchpad to collect ideas.

A D-Star Service Gateway is used to connect Packet Applications to a wider network.

Our main goals
  * Make WL2K work over D-Star DV data
  * DXCluster?
  * Dedicated channel for D-Rats Ratflector (one for local reflection, another for bridging to a network Ratflector)

## General architecture

```mermaid
flowchart LR
    wl2k[/Winlink/] <-->|kiss| tnc@{ shape: stadium, label: "DTTNC"}
    rat[/D-RATS/] <-->|kiss| tnc
    tnc <-->|CI-V| radio[Radio]
    radio <-..-> rptr[Repeater]
    rptr <-->|DExtra| dsgw@{ shape: stadium, label: "S-GW"}
    dsgw <-->|RMS2CMS OpenB2F| cms[/Winlink CMS/]
    dsgw <-->|telnet| dxc[/DXCluster/]
    dsgw <--> |telnet| rfl[/Ext. Ratflector/]
    dsgw --> |Builtin Ratflector| dsgw
```
