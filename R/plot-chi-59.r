#!/usr/bin/Rscript

chi.df = 59

chi.density <- function(x) { dchisq(x, chi.df) }

chi.critical <- function(x) { qchisq(x, chi.df) }

chi.tail <- function(x) { pchisq(x, chi.df, lower.tail=FALSE) }

args = commandArgs(TRUE)
estatistica = as.numeric(args[1])
if (is.na(estatistica)) estatistica = chi.critical(0.5)

#x11(display=":0.0", family='serif', 12.7, 6, 9)
png(filename='chi-59.png', width=640, height=480, family='DejaVu Serif', pointsize=9)

opar=par(bg='white', fg='#333333')

curve(
  chi.density,
  xlim=c(0, 120),
  ylim=c(0, 0.04),
  bty='n',          # renderiza apenas eixos
  col='blue',
  main = list(
    sprintf("Distribuição χ² (gl=%d)", chi.df),
    cex=1.75,   # font size scale
    font=4      # bold + italic
  ),
  ylab = list(''),
  xlab = list('')
)

valores <- c(estatistica, chi.critical(0.95))
cores <- c('#00cc00', '#cc0000')

abline(v=valores, lty='solid', col=cores)

gd <- par()$usr # coordenadas dos extremos da área de renderização da função

legend(
  2*gd[2]/3, 4*gd[4]/5,
  bty='n',
  cex=1.5,
  text.font=2,
  lty='solid',
  col=cores,
  legend=c(sprintf("%.3f (%5.3f)", valores, chi.tail(valores)))
)

# renderiza hachuras num intervalo arbitrário
hatch <- function(from, to, color) {
  a = from
  while (a <= to) {
    y = chi.density(a)
    if (y < 1E-4) break
    segments(a, 0, a, y, col=color, lty='solid', lwd=1)
    a = a + 0.35
  }
}

hatch(valores[1], valores[2], cores[1])

hatch(valores[2], gd[2], cores[2])

dev.off()