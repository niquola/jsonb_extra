--db:test
--{{{
DROP EXTENSION nicola;
CREATE EXTENSION nicola;

SELECT nicola('{"a":"1", "b":{"c":"2", "d": [1,2,{"x":100}], "e":{"f": 4}}}', '{b,e,f}');
SELECT nicola('{"b":{"c":"2"}}', '{b,c}');
SELECT nicola('{"b":[{"c":1},{"c":2},{"x":"ups"}]}', '{b,c}');


SELECT nicola('{"b":[{"c":{"x": 5}},{"c":2},{"x":"ups"}]}', '{b,c}');

--}}}
