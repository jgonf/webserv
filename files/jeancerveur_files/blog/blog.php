<!doctype html>
<html>
  <head>
    <title>A super blog</title>
  </head>
  <body>
		<h1>Jeancerveur's blog</h1>
		<h2>I had a dream</h2>
		<p>L'autre jour j'ai fait un rêve. On validait webserv. C'était bien.</p>
		<p>Written by Célia</p>
		</br>
		
		<?php
			if ($SERVER['title'])
			{
				echo "<h2>";
				echo $SERVER['title'];
				echo "</h2>";
			}
			if ($SERVER['content'])
			{
				echo "<p>";
				echo $SERVER['content'];
				echo "</p>";
			}
			if ($SERVER['name'])
			{
				echo "<p>Written by ";
				echo $SERVER['name'];
				echo "</p>";
			}
		?>
		
		<h3>Why don't you submit your own article ?</h3>
		<form action="blog.php" method="post">
			Title <input type="text" name="title"><br />
			Content <input type="text" name="content"><br />
			Your name <input type="text" name="name"><br />
		<input type="submit" value="Submit info">
		</form>
	
  </body>
</html>