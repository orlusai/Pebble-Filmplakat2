function s(e) {
	for (o={}, i=0; i<e.length; i++)
		(j=e[i].id) && (o[j] = e[i].checked ? 1 : 0);
	return window.location.href="pebblejs://close#"+JSON.stringify(o),!1
}

var d=JSON.parse(decodeURIComponent(window.location.hash.substring(1)));
for(var i in d)
	d.hasOwnProperty(i) && (document.getElementById(i).checked=!!d[i]);
