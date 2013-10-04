function initialize(){
  comboobj=document.all? document.all.staticcombo : document.getElementById? document.getElementById("staticcombo") : document.layers? document.staticcombo : 0
  windowwidth=window.innerWidth? window.innerWidth-30 : document.body.clientWidth-20
  windowheight=window.innerHeight? window.innerHeight : document.body.clientHeight
  if (!comboobj)
    return
  if (document.all || document.getElementById){
    combowidth=comboobj.offsetWidth
    comboheight=comboobj.offsetHeight
    setInterval("staticit_dom()",50)
    comboobj.style.visibility="visible"
  }
  else if (document.layers){
    combowidth=comboobj.document.width
    comboheight=comboobj.document.height
    setInterval("staticit_ns()",50)
    comboobj.visibility="show"
  }
}

function staticit_dom(){
  var pageoffsetx=document.all? document.body.scrollLeft : window.pageXOffset
  var pageoffsety=document.all? document.body.scrollTop : window.pageYOffset
  comboobj.style.left=pageoffsetx
  comboobj.style.top=pageoffsety
}

function staticit_ns(){
  comboobj.left=pageXOffset
  comboobj.top=pageYOffset
}

window.onload=initialize
