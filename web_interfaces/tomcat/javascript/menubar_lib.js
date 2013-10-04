/*====================================
  DyNAMiC-cOre.Net
   Toolbar
====================================*/
var Toolbar='<DIV ID="Toolbar" class="tbToolbar" NOWRAP><table border=0><tr><!-- TB_ITEMS --></tr></table></DIV><!-- TB_SUBITEMS -->'
var currentMenu=null
var currentItem=null
var focusTimer=null
   
function drawToolbar() { document.write(Toolbar) }
function addMenu(id,caption) {
	var tmpTag='<!-- TB_ITEMS -->'
	Toolbar=Toolbar.replace(tmpTag,'<TD id="IT_'+id+'" class="tbItem" NOWRAP><p class="tbItemLink" onmouseover="doAction(\''+id+'\')" onmousemove="setFocus(\''+id+'\')">'+caption+'</p></td><td width="5px">&nbsp;</td>'+tmpTag)
	var tmpTag='<!-- TB_SUBITEMS -->'
	Toolbar=Toolbar.replace(tmpTag,'<DIV id="TBS_'+id+'" class="tbSubMenu" onmousemove="setFocus(\''+id+'\')" NOWRAP><!-- TBS_'+id+' --></div>'+tmpTag)
}
function addMenuItem(p,id,caption,turl,hint) {
	var tmpTag='<!-- TBS_'+p+' -->'
	Toolbar=Toolbar.replace(tmpTag,'<DIV id="TBSI_'+id+'" class="tbSubItemLink" onmouseover="showSelect(\''+id+'\',\''+hint+'\')" onmouseup="location=\''+turl+'\'" NOWRAP>'+caption+'</div>'+tmpTag)
}
function addSubMenu(p,id,caption,url) {
	var tmpTag='<!-- TB_SUBITEMS -->'
	Toolbar=Toolbar.replace(tmpTag,'<DIV id="TBSUB_'+p+'" class="tbSubMenu" onmousemove="setFocus(\''+id+'\')" NOWRAP><!-- TBS_'+id+' --></div>'+tmpTag)
}
function setFocus(id) {
	if (focusTimer) clearTimeout(focusTimer)
	focusTimer=setTimeout('doHide("'+id+'")',1000)
}
function doHide(id) {
	var o=document.getElementById('TBS_'+id)
	if (!o) var o=document.getElementById('TBSUB_'+id)
	o.style.display="none"

	var o=document.getElementById('IT_'+id)
	o.className='tbItem'
	
	if (currentItem) document.getElementById('TBSI_'+currentItem).className="tbSubItemLink"
}
function showSelect(id,hint) {
	if (currentItem) document.getElementById('TBSI_'+currentItem).className="tbSubItemLink"
	document.getElementById('TBSI_'+id).className="tbSubItemLinkSelected"
	currentItem=id
	if (hint!='undefined') status=hint
	else status=''

	var o=document.getElementById('Toolbar')
	y=o.offsetTop+o.offsetHeight-4

	// check for SubMenu and display it	
	var o=document.getElementById('TBSUB_'+id)
	if (o) {
		y+=document.getElementById('TBSI_'+id).offsetTop+document.getElementById('TBSI_'+id).offsetHeight-4
		x=document.getElementById('TBSI_'+id).offsetLeft+document.getElementById('TBSI_'+id).offsetParent.offsetLeft+document.getElementById('TBSI_'+id).offsetParent.offsetWidth-15
		//o.className='tbItemSelected'
	
		//o=document.getElementById('TBS_'+id)
		o.style.display='block'	
		o.style.left=x
		o.style.top=y
	
		currentMenu=o.id
	}
	
}
function doAction(id) {
	var o=document.getElementById('Toolbar')
	y=o.offsetTop+o.offsetHeight-4
	
	if (currentMenu) {
		document.getElementById('IT_'+currentMenu).className='tbItem'
		document.getElementById('TBS_'+currentMenu).style.display='none'
	}

	var o=document.getElementById('IT_'+id)
	x=o.offsetLeft+o.offsetParent.offsetLeft;
	o.className='tbItemSelected'
	
	o=document.getElementById('TBS_'+id)
	o.style.display='block'	
	o.style.left=x
	o.style.top=y
	
	currentMenu=id
}
