-- estatística do teste de aderência chi-quadrado para verificar se as
-- frequências das dezenas seguem distribuição uniforme
SELECT
  sum(desvio*desvio/esperanca) AS chi,  -- estatística do teste
  count(*)-1 AS gl                      -- graus de liberdade
FROM (
  SELECT
    esperanca,
    (frequencia-esperanca) AS desvio    -- desvio das classes
  FROM info_dezenas, (
    SELECT avg(frequencia) AS esperanca -- valor constante
    FROM info_dezenas
  )
);
