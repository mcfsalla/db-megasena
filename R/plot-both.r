#!/usr/bin/Rscript
library(RSQLite, quietly=TRUE)
con <- sqliteNewConnection(dbDriver('SQLite'), dbname='megasena.sqlite')

rs <- dbSendQuery(con, 'SELECT dezena FROM dezenas_sorteadas')
datum <- fetch(rs, n = -1)

nrec = length(datum$dezena) / 6;
titulo = sprintf('Frequências das dezenas #%d', nrec)

# monta a tabela das classes com suas respectivas frequências
tabela <- table(datum$dezena)

# prepara os rótulos das classes formatando os números das dezenas
rotulos <- c(sprintf('%02d', 1:60))
dimnames(tabela) <- list(rotulos)

# arquivo para armazenamento da imagem com declaração das dimensões do
# device gráfico e tamanho da fonte de caracteres
fname=sprintf('img/both-%d.png', nrec)
png(filename=fname, width=1300, height=558, pointsize=9)

# preserva configuração do device gráfico antes de personalizar
op = par(mfrow=c(1, 2))  # dois gráficos alinhados horizontalmente

barplot(
  tabela,
  main=titulo,
  ylab='frequência',
  col=c('gold', 'orange'),
  space=.25,
  ylim=c(0, (1+max(tabela)%/%25)*25)
)

# sobrepõe linha horizontal de referência
abline(
  h=mean(tabela), # esperança das frequências observadas
  col='red',      # cor da linha
  lty=3           # 1=continua, 2=tracejada, 3=pontilhada
)

gd <- par()$usr   # coordenadas dos extremos do dispositivo de renderização

legend(
  3*(gd[1]+gd[2])/4, gd[4],   # coordenada (x,y) da legenda
  bty='n',                    # omite renderização de bordas
  col='red', lty=3,           # atributos da única linha amostrada
  legend=c('esperança')       # texto correspondente da linha
)

rs <- dbSendQuery(con, 'SELECT latencia FROM info_dezenas')
datum <- fetch(rs, n = -1)

dbClearResult(rs)
sqliteCloseConnection(con)

titulo = sprintf('Latências das dezenas #%d', nrec)

x <- as.vector(datum$latencia)

names(x) <- rotulos

barplot(
  x,
  main=titulo,
  ylab='latência',
  col=c('gold', 'orange')
)

abline(
  h=10,             # esperança das latências
  col='red', lty=3
)

gd <- par()$usr

legend(
  3*(gd[1]+gd[2])/4, 4*gd[4]/5,
  bty='n',
  col='red', lty=3,
  legend=c('esperança')
)

par = op  # restaura device gráfico

dev.off()   # finaliza a renderização e fecha o arquivo