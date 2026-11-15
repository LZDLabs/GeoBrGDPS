begin;
do $$
declare sid bigint;
begin
 insert into public.geobr_pass_seasons(name,starts_at,ends_at,xp_per_tier,published)
 values('GeoBR - Expedicao',now(),now()+interval '30 days',100,false) returning id into sid;
 update public.geobr_quest_steps set published=false where season_id=sid;
 update public.geobr_quest_steps q set title=v.title,kind=v.kind,goal=v.goal,difficulty=v.diff,scope=v.scope,mode=v.mode,platformer=v.plat,min_percent=v.lo,max_percent=v.hi,xp=v.xp,tokens=v.tokens,published=true
 from (values
 (1,'Primeiro passo: complete um nivel','complete',1,'any','any','normal','any',100,100,100,10),
 (2,'Explore: alcance 50% em 2 niveis','reach',2,'any','online','normal','classic',50,100,100,10),
 (3,'Dupla Easy: complete 2 diferentes','complete',2,'easy','online','normal','any',100,100,125,12),
 (4,'Treino: conclua uma pratica','complete',1,'any','online','practice','classic',100,100,100,10),
 (5,'Campanha: complete um Main Level','complete',1,'any','main','normal','any',100,100,125,12),
 (6,'Plataforma: conclua um nivel','complete',1,'any','online','normal','platformer',100,100,125,12),
 (7,'Normal: complete 2 diferentes','complete',2,'normal','online','normal','classic',100,100,125,12),
 (8,'Quase la: alcance 80%','reach',1,'any','online','normal','classic',80,100,125,12),
 (9,'Hard: conclua um nivel','complete',1,'hard','online','normal','classic',100,100,150,15),
 (10,'Classicos: complete 2 oficiais','complete',2,'any','official','normal','classic',100,100,125,12),
 (11,'Explore 3 niveis ate 60%','reach',3,'any','online','normal','classic',60,100,150,15),
 (12,'Dupla de plataformas','complete',2,'any','online','normal','platformer',100,100,150,15),
 (13,'Treine um Demon','complete',1,'demon','online','practice','classic',100,100,150,15),
 (14,'Harder: conclua um nivel','complete',1,'harder','online','normal','classic',100,100,150,15),
 (15,'Campanha: chegue a 75%','reach',1,'any','main','normal','classic',75,100,125,12),
 (16,'Colecao: complete 3 diferentes','complete',3,'any','online','normal','any',100,100,175,18),
 (17,'Insane: chegue a 50%','reach',1,'insane','online','normal','classic',50,100,150,15),
 (18,'Mais treino: 2 praticas diferentes','complete',2,'any','online','practice','classic',100,100,150,15),
 (19,'Precisao: alcance 90%','reach',1,'any','online','normal','classic',90,100,150,15),
 (20,'Dupla Hard','complete',2,'hard','online','normal','classic',100,100,175,18),
 (21,'Aventureiro: 3 plataformas','complete',3,'any','online','normal','platformer',100,100,175,18),
 (22,'Complete 3 oficiais diferentes','complete',3,'any','official','normal','classic',100,100,150,15),
 (23,'Desafio Insane: conclua um','complete',1,'insane','online','normal','classic',100,100,200,20),
 (24,'Expedicao: complete 5 diferentes','complete',5,'any','online','normal','any',100,100,200,20)
 ) v(n,title,kind,goal,diff,scope,mode,plat,lo,hi,xp,tokens)
 where q.season_id=sid and q.sequence_order=v.n;
 update public.geobr_pass_rewards r set kind=v.kind,amount=v.amount,icon_type=v.icon_type,icon_id=v.icon
 from (values
 (1,'tokens',30,'cube',1),(2,'skin',1,'cube',25),(3,'orbs',200,'cube',1),
 (4,'skin',1,'ship',12),(5,'tokens',40,'cube',1),(6,'skin',1,'ball',8),
 (7,'orbs',300,'cube',1),(8,'skin',1,'ufo',10),(9,'tokens',50,'cube',1),
 (10,'skin',1,'wave',12),(11,'orbs',400,'cube',1),(12,'skin',1,'robot',8),
 (13,'tokens',60,'cube',1),(14,'stars',3,'cube',1),(15,'skin',1,'spider',6),
 (16,'orbs',500,'cube',1),(17,'tokens',70,'cube',1),(18,'skin',1,'swing',5),
 (19,'orbs',600,'cube',1),(20,'tokens',80,'cube',1),(21,'skin',1,'jetpack',3),
 (22,'stars',5,'cube',1),(23,'tokens',90,'cube',1),(24,'skin',1,'cube',50),
 (25,'orbs',800,'cube',1),(26,'tokens',100,'cube',1),(27,'skin',1,'ship',20),
 (28,'stars',8,'cube',1),(29,'tokens',125,'cube',1),(30,'skin',1,'cube',100)
 ) v(t,kind,amount,icon_type,icon) where r.season_id=sid and r.tier=v.t;
 update public.geobr_pass_seasons set published=true where id=sid;
end $$;
commit;
