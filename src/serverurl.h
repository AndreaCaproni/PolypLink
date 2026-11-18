#ifndef SERVERURL_H
#define SERVERURL_H

#include <QString>

// Normalizza l'indirizzo del server Jellyfin.
//
// Se lo schema manca, usa HTTPS per host pubblici e HTTP per host della rete
// locale (IP privati, localhost, nomi .local/.lan/.home/.internal o nomi senza
// dominio). Un server LAN tipico ascolta su HTTP:8096; un server pubblico deve
// usare HTTPS per non spedire credenziali e token in chiaro.
QString NormalizeServerUrl(QString url);

#endif  // SERVERURL_H
